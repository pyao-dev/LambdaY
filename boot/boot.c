#include <efi.h>
#include <efilib.h>

EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *system_table) {
    InitializeLib(image, system_table);
    Print(L"Welcome to LambdaY Operating System!\r\n");

    while (1)
        ; // never breaks
    return EFI_SUCCESS;
}
