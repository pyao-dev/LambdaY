#include <efi.h>
#include <efilib.h>

EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE* system_table) {
    EFI_STATUS                       status;
    EFI_LOADED_IMAGE*                loaded_image;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* file_system;
    EFI_FILE_HANDLE                  root;
    EFI_FILE_HANDLE                  kernel_file;
    EFI_FILE_INFO*                   file_info;
    VOID*                            kernel_buffer;
    UINTN                            info_size;
    UINTN                            kernel_size;
    EFI_HANDLE                       kernel_image;

    InitializeLib(image, system_table);
    Print(L"Welcome to LambdaY Operating System!\r\n");

    status = uefi_call_wrapper(BS->HandleProtocol, 3, image, &LoadedImageProtocol, (VOID**)&loaded_image);
    if (EFI_ERROR(status)) {
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
    status = uefi_call_wrapper(kernel_file->GetInfo, 4, kernel_file, &GenericFileInfo, &info_size, NULL);
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

    kernel_size = (UINTN)file_info->FileSize;
    FreePool(file_info);
    kernel_buffer = AllocatePool(kernel_size);
    if (kernel_buffer == NULL) {
        uefi_call_wrapper(kernel_file->Close, 1, kernel_file);
        return EFI_OUT_OF_RESOURCES;
    }

    status = uefi_call_wrapper(kernel_file->Read, 3, kernel_file, &kernel_size, kernel_buffer);
    uefi_call_wrapper(kernel_file->Close, 1, kernel_file);
    if (EFI_ERROR(status)) {
        Print(L"Unable to read kernel.bin: %r\r\n", status);
        FreePool(kernel_buffer);
        return status;
    }

    status = uefi_call_wrapper(BS->LoadImage, 6, FALSE, image, NULL, kernel_buffer, kernel_size, &kernel_image);
    FreePool(kernel_buffer);
    if (EFI_ERROR(status)) {
        Print(L"Unable to load kernel.bin: %r\r\n", status);
        return status;
    }

    Print(L"Starting kernel...\r\n");
    status = uefi_call_wrapper(BS->StartImage, 3, kernel_image, NULL, NULL);
    if (EFI_ERROR(status)) {
        Print(L"Kernel returned with status: %r\r\n", status);
    }

    return status;
}
