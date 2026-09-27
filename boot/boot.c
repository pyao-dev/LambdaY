#include <efi.h>
#include <efilib.h>
#include <pe.h>

#include <boot_info.h>

#define KERNEL_MAX_SIZE (16 * 1024 * 1024)
#define MEMORY_MAP_SLACK_DESCRIPTORS 8

typedef struct {
    UINT32            signature;
    IMAGE_FILE_HEADER file_header;
} PE_NT_HEADER_PREFIX;

typedef struct {
    UINT16 magic;
    UINT8  data[14];
    UINT32 address_of_entry_point;
} PE_OPTIONAL_HEADER_PREFIX;

typedef void(__attribute__((ms_abi)) * KERNEL_ENTRY)(const BootInfo* boot_info);

static BOOLEAN range_is_valid(UINTN offset, UINTN length, UINTN total) {
    return offset <= total && length <= total - offset;
}

static EFI_STATUS find_kernel_entry(EFI_LOADED_IMAGE* loaded_image, KERNEL_ENTRY* entry) {
    if (loaded_image == NULL || loaded_image->ImageBase == NULL || loaded_image->ImageSize < sizeof(IMAGE_DOS_HEADER) ||
        entry == NULL) {
        return EFI_LOAD_ERROR;
    }

    UINT8*            image      = (UINT8*)loaded_image->ImageBase;
    UINTN             image_size = (UINTN)loaded_image->ImageSize;
    IMAGE_DOS_HEADER* dos        = (IMAGE_DOS_HEADER*)image;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew > image_size - sizeof(PE_NT_HEADER_PREFIX)) {
        return EFI_LOAD_ERROR;
    }

    PE_NT_HEADER_PREFIX* nt = (PE_NT_HEADER_PREFIX*)(image + dos->e_lfanew);
    if (nt->signature != IMAGE_NT_SIGNATURE || nt->file_header.Machine != IMAGE_FILE_MACHINE_X64 ||
        nt->file_header.SizeOfOptionalHeader < sizeof(PE_OPTIONAL_HEADER_PREFIX) ||
        !range_is_valid(dos->e_lfanew + sizeof(PE_NT_HEADER_PREFIX), nt->file_header.SizeOfOptionalHeader,
                        image_size)) {
        return EFI_LOAD_ERROR;
    }

    PE_OPTIONAL_HEADER_PREFIX* optional =
        (PE_OPTIONAL_HEADER_PREFIX*)(image + dos->e_lfanew + sizeof(PE_NT_HEADER_PREFIX));
    if (optional->magic != 0x20b || optional->address_of_entry_point >= image_size) {
        return EFI_LOAD_ERROR;
    }

    *entry = (KERNEL_ENTRY)(image + optional->address_of_entry_point);
    return EFI_SUCCESS;
}

static EFI_STATUS get_framebuffer(BootInfo* boot_info) {
    EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = NULL;
    EFI_STATUS status = uefi_call_wrapper(BS->LocateProtocol, 3, &GraphicsOutputProtocol, NULL, (VOID**)&gop);
    if (EFI_ERROR(status) || gop == NULL || gop->Mode == NULL || gop->Mode->Info == NULL) {
        return EFI_NOT_FOUND;
    }

    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* info = gop->Mode->Info;
    if (gop->Mode->FrameBufferBase == 0 || gop->Mode->FrameBufferSize == 0 || info->HorizontalResolution == 0 ||
        info->VerticalResolution == 0 || info->PixelsPerScanLine < info->HorizontalResolution ||
        info->PixelFormat > PixelBlueGreenRedReserved8BitPerColor) {
        return EFI_DEVICE_ERROR;
    }

    boot_info->framebuffer.address             = gop->Mode->FrameBufferBase;
    boot_info->framebuffer.size                = gop->Mode->FrameBufferSize;
    boot_info->framebuffer.width               = info->HorizontalResolution;
    boot_info->framebuffer.height              = info->VerticalResolution;
    boot_info->framebuffer.pixels_per_scanline = info->PixelsPerScanLine;
    boot_info->framebuffer.pixel_format =
        info->PixelFormat == PixelRedGreenBlueReserved8BitPerColor ? BootPixelFormatRgb : BootPixelFormatBgr;
    return EFI_SUCCESS;
}

static EFI_STATUS allocate_memory_map(EFI_MEMORY_DESCRIPTOR** map, UINTN* map_size, UINTN* map_key,
                                      UINTN* descriptor_size, UINT32* descriptor_version) {
    EFI_STATUS status;
    UINTN      required_size            = 0;
    UINTN      local_key                = 0;
    UINTN      local_descriptor_size    = 0;
    UINT32     local_descriptor_version = 0;

    status = uefi_call_wrapper(BS->GetMemoryMap, 5, &required_size, NULL, &local_key, &local_descriptor_size,
                               &local_descriptor_version);
    if (status != EFI_BUFFER_TOO_SMALL || local_descriptor_size == 0) {
        return status;
    }

    if (required_size > (~(UINTN)0) - MEMORY_MAP_SLACK_DESCRIPTORS * local_descriptor_size) {
        return EFI_OUT_OF_RESOURCES;
    }
    required_size += MEMORY_MAP_SLACK_DESCRIPTORS * local_descriptor_size;
    *map = AllocatePool(required_size);
    if (*map == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    *map_size           = required_size;
    *map_key            = local_key;
    *descriptor_size    = local_descriptor_size;
    *descriptor_version = local_descriptor_version;
    return EFI_SUCCESS;
}

static UINTN descriptor_count(UINTN map_size, UINTN descriptor_size) {
    return descriptor_size == 0 ? 0 : map_size / descriptor_size;
}

static void copy_memory_map(BootInfo* boot_info, EFI_MEMORY_DESCRIPTOR* map, UINTN map_size, UINTN descriptor_size) {
    UINTN count                 = descriptor_count(map_size, descriptor_size);
    boot_info->memory_map_count = count;

    for (UINTN index = 0; index < count; ++index) {
        EFI_MEMORY_DESCRIPTOR* source = (EFI_MEMORY_DESCRIPTOR*)((UINT8*)map + index * descriptor_size);
        MemoryRegion*          target = &boot_info->memory_map[index];
        target->type                  = source->Type;
        target->reserved              = 0;
        target->physical_start        = source->PhysicalStart;
        target->virtual_start         = source->VirtualStart;
        target->page_count            = source->NumberOfPages;
        target->attributes            = source->Attribute;
    }
}

EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE* system_table) {
    EFI_STATUS                       status;
    EFI_LOADED_IMAGE*                loaded_image;
    EFI_LOADED_IMAGE*                kernel_loaded_image;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* file_system;
    EFI_FILE_HANDLE                  root;
    EFI_FILE_HANDLE                  kernel_file;
    EFI_FILE_INFO*                   file_info;
    VOID*                            kernel_buffer;
    UINTN                            info_size;
    UINTN                            kernel_size;
    EFI_HANDLE                       kernel_image;
    BootInfo*                        boot_info;
    EFI_MEMORY_DESCRIPTOR*           memory_map         = NULL;
    UINTN                            memory_map_size    = 0;
    UINTN                            map_key            = 0;
    UINTN                            descriptor_size    = 0;
    UINT32                           descriptor_version = 0;
    KERNEL_ENTRY                     kernel_entry       = NULL;

    InitializeLib(image, system_table);
    Print(L"Welcome to LambdaY Operating System!\r\n");

    status = uefi_call_wrapper(BS->HandleProtocol, 3, image, &LoadedImageProtocol, (VOID**)&loaded_image);
    if (EFI_ERROR(status) || loaded_image == NULL || loaded_image->DeviceHandle == NULL) {
        Print(L"Unable to get loaded image protocol: %r\r\n", status);
        return status;
    }

    status =
        uefi_call_wrapper(BS->HandleProtocol, 3, loaded_image->DeviceHandle, &FileSystemProtocol, (VOID**)&file_system);
    if (EFI_ERROR(status)) {
        Print(L"Unable to get file system protocol: %r\r\n", status);
        return status;
    }

    status = uefi_call_wrapper(file_system->OpenVolume, 2, file_system, &root);
    if (EFI_ERROR(status)) {
        Print(L"Unable to open file system volume: %r\r\n", status);
        return status;
    }

    status = uefi_call_wrapper(root->Open, 5, root, &kernel_file, L"\\kernel.bin", EFI_FILE_MODE_READ, 0);
    uefi_call_wrapper(root->Close, 1, root);
    if (EFI_ERROR(status)) {
        Print(L"Unable to open kernel.bin: %r\r\n", status);
        return status;
    }

    info_size = 0;
    status    = uefi_call_wrapper(kernel_file->GetInfo, 4, kernel_file, &GenericFileInfo, &info_size, NULL);
    if (status != EFI_BUFFER_TOO_SMALL) {
        Print(L"Unable to get kernel.bin information: %r\r\n", status);
        uefi_call_wrapper(kernel_file->Close, 1, kernel_file);
        return status;
    }

    file_info = AllocatePool(info_size);
    if (file_info == NULL) {
        uefi_call_wrapper(kernel_file->Close, 1, kernel_file);
        return EFI_OUT_OF_RESOURCES;
    }

    status = uefi_call_wrapper(kernel_file->GetInfo, 4, kernel_file, &GenericFileInfo, &info_size, file_info);
    if (EFI_ERROR(status)) {
        Print(L"Unable to read kernel.bin information: %r\r\n", status);
        FreePool(file_info);
        uefi_call_wrapper(kernel_file->Close, 1, kernel_file);
        return status;
    }

    if (file_info->FileSize == 0 || file_info->FileSize > KERNEL_MAX_SIZE) {
        Print(L"kernel.bin has an invalid size: %lu\r\n", file_info->FileSize);
        FreePool(file_info);
        uefi_call_wrapper(kernel_file->Close, 1, kernel_file);
        return EFI_BAD_BUFFER_SIZE;
    }

    kernel_size = (UINTN)file_info->FileSize;
    FreePool(file_info);
    kernel_buffer = AllocatePool(kernel_size);
    if (kernel_buffer == NULL) {
        uefi_call_wrapper(kernel_file->Close, 1, kernel_file);
        return EFI_OUT_OF_RESOURCES;
    }

    UINTN bytes_read = kernel_size;
    status           = uefi_call_wrapper(kernel_file->Read, 3, kernel_file, &bytes_read, kernel_buffer);
    uefi_call_wrapper(kernel_file->Close, 1, kernel_file);
    if (EFI_ERROR(status) || bytes_read != kernel_size) {
        Print(L"Unable to read kernel.bin: %r\r\n", status);
        FreePool(kernel_buffer);
        return EFI_LOAD_ERROR;
    }

    status = uefi_call_wrapper(BS->LoadImage, 6, FALSE, image, NULL, kernel_buffer, kernel_size, &kernel_image);
    FreePool(kernel_buffer);
    if (EFI_ERROR(status)) {
        Print(L"Unable to load kernel.bin: %r\r\n", status);
        return status;
    }

    status = uefi_call_wrapper(BS->HandleProtocol, 3, kernel_image, &LoadedImageProtocol, (VOID**)&kernel_loaded_image);
    if (EFI_ERROR(status)) {
        Print(L"Unable to inspect loaded kernel: %r\r\n", status);
        return status;
    }

    status = find_kernel_entry(kernel_loaded_image, &kernel_entry);
    if (EFI_ERROR(status)) {
        Print(L"Invalid kernel entry point: %r\r\n", status);
        return status;
    }

    boot_info = AllocatePool(sizeof(BootInfo));
    if (boot_info == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }
    *boot_info         = (BootInfo){0};
    boot_info->magic   = LAMBDAY_BOOT_INFO_MAGIC;
    boot_info->version = LAMBDAY_BOOT_INFO_VERSION;
    boot_info->size    = sizeof(BootInfo);

    status = get_framebuffer(boot_info);
    if (EFI_ERROR(status)) {
        Print(L"Unable to capture framebuffer information: %r\r\n", status);
        return status;
    }

    status = allocate_memory_map(&memory_map, &memory_map_size, &map_key, &descriptor_size, &descriptor_version);
    if (EFI_ERROR(status)) {
        Print(L"Unable to allocate memory map: %r\r\n", status);
        return status;
    }

    UINTN max_descriptors = descriptor_count(memory_map_size, descriptor_size);
    boot_info->memory_map = AllocatePool(max_descriptors * sizeof(MemoryRegion));
    if (boot_info->memory_map == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    status = uefi_call_wrapper(BS->SetWatchdogTimer, 4, 0, 0, 0, NULL);
    if (EFI_ERROR(status)) {
        Print(L"Unable to disable watchdog: %r\r\n", status);
        return status;
    }

    for (;;) {
        memory_map_size = max_descriptors * descriptor_size;
        status = uefi_call_wrapper(BS->GetMemoryMap, 5, &memory_map_size, memory_map, &map_key, &descriptor_size,
                                   &descriptor_version);
        if (status == EFI_BUFFER_TOO_SMALL) {
            Print(L"Memory map grew unexpectedly.\r\n");
            return status;
        }
        if (EFI_ERROR(status)) {
            Print(L"Unable to get final memory map: %r\r\n", status);
            return status;
        }

        copy_memory_map(boot_info, memory_map, memory_map_size, descriptor_size);
        status = uefi_call_wrapper(BS->ExitBootServices, 2, image, map_key);
        if (status == EFI_INVALID_PARAMETER) {
            continue;
        }
        break;
    }

    if (EFI_ERROR(status)) {
        return status;
    }

    kernel_entry(boot_info);
    for (;;) {
    }
}
