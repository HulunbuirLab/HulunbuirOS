#include <efi.h>
#include <efilib.h>
#include <stdbool.h>

#include "bootloader/elf_parse.h"
#include "bootloader/fileops.h"
#include "kernel/include/xtos.h"

#include "bootloader/efiConsoleControl.h"

EFI_STATUS
get_memory_map(OUT void **map, OUT UINTN *mem_map_size, OUT UINTN *mem_map_key,
               OUT UINTN *descriptor_size) {
  EFI_STATUS status;
  *mem_map_size = sizeof(EFI_MEMORY_DESCRIPTOR) * 48;
  UINTN mem_map_descriptor_version;

  // Keep trying larger buffers until we find one large enough to hold the map
   while (1) {
      status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData,
                                 *mem_map_size, map);

      if (EFI_ERROR(status)) {
        Print(L"AllocatePool() error: %d (tried to allocate %d bytes)\n", status,
              *mem_map_size);
        return status;
      }

     //*map = malloc(*mem_map_size);
    Print(L"Trying to get memory map with size: %d, buffer: %x...\n",
          *mem_map_size, *map);
    status =
        uefi_call_wrapper(BS->GetMemoryMap, 5, mem_map_size, *map, mem_map_key,
                          descriptor_size, &mem_map_descriptor_version);

    // We can only recover from an EFI_BUFFER_TOO_SMALL error
    if (status == EFI_BUFFER_TOO_SMALL) {
      /*
        `mem_map_size` will point to the size needed if the previous call failed
        but we want to reserve a bit more space than we needed for the last
        call in case the new allocation changed the memory map.
      */
       status = uefi_call_wrapper(BS->FreePool, 1, *map);
//       free(*map);
      *mem_map_size += sizeof(EFI_MEMORY_DESCRIPTOR) * 16;
    } else {
      return status;  // It's up to caller to check for an error
    }
  }
}

void wait_for_keypress() {
  UINTN event_index;
  EFI_EVENT events[1] = {ST->ConIn->WaitForKey};
  uefi_call_wrapper(BS->WaitForEvent, 3, 1, events, &event_index);
}

bool guids_match(EFI_GUID guid1, EFI_GUID guid2) {
  bool first_part_good =
      (guid1.Data1 == guid2.Data1 && guid1.Data2 == guid2.Data2 &&
       guid1.Data3 == guid2.Data3);

  if (!first_part_good) return false;

  for (int i = 0; i < 8; ++i) {
    if (guid1.Data4[i] != guid2.Data4[i]) return false;
  }

  return true;
}

EFI_GUID gEfiConsoleControlProtocolGuid = EFI_CONSOLE_CONTROL_PROTOCOL_GUID;

EFI_STATUS
EFIAPI
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
  EFI_STATUS status;

  InitializeLib(ImageHandle, SystemTable);

  uefi_call_wrapper(BS->SetWatchdogTimer, 4, 0, 0x0, 0, NULL);
  uefi_call_wrapper(ST->ConOut->Reset, 2, ST->ConOut, FALSE);

  EFI_CONSOLE_CONTROL_PROTOCOL *ConsoleControl;

  if (uefi_call_wrapper(BS->LocateProtocol, 3, &gEfiConsoleControlProtocolGuid,
                        NULL, &ConsoleControl) == EFI_SUCCESS) {
    uefi_call_wrapper(ConsoleControl->SetMode, 2, ConsoleControl,
                      EfiConsoleControlScreenText);
  }

  fops_init(ImageHandle);

  // Load the kernel ELF file into memory and get the entry address
  void *kernel_main_addr = NULL;
  status = load_kernel(L"kernel", &kernel_main_addr);
  if (status != EFI_SUCCESS) {
    Print(L"Error loading kernel: %d\n", status);
    return EFI_ABORTED;
  }

  Print(L"Got kernel_main at: 0x%x\n", kernel_main_addr);

  EFI_CONFIGURATION_TABLE *configuration_tables =
      SystemTable->ConfigurationTable;

  void *xdsp_address = NULL;
  static EFI_GUID acpi_guid = ACPI_20_TABLE_GUID;
  for (unsigned i = 0; i < SystemTable->NumberOfTableEntries; ++i) {
    if (guids_match(acpi_guid, configuration_tables[i].VendorGuid)) {
      Print(L"Found ACPI Table pointer 0x%x\n",
            configuration_tables[i].VendorTable);
      xdsp_address = configuration_tables[i].VendorTable;
    }
  }

  if (!xdsp_address) {
    Print(L"Could not locate ACPI v2.0 table, aborting!\n");
    return EFI_ABORTED;
  }

  // Get access to a simple graphics buffer
  // Try to close graphic version
  EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
  EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
  
  UINTN BufferSize = 0;
  EFI_HANDLE *HandleBuffer = NULL;
  status = uefi_call_wrapper(BS->LocateHandleBuffer, 5, ByProtocol, &gop_guid, NULL, &BufferSize, &HandleBuffer);
  if (EFI_ERROR(status)) {
    Print(
      L"Could not locate Simple Graphics Output Protocol handle, error: %d. "
      L"Aborting.\n",
      status);
    return EFI_ABORTED;
  }
  status = uefi_call_wrapper(BS->HandleProtocol, 3, HandleBuffer[0], &gop_guid, &gop);
  if (EFI_ERROR(status)) {
    Print(
      L"Could not handle Simple Graphics Output Protocol, error: %d. "
      L"Aborting.\n",
      status);
    return EFI_ABORTED;
  }
  
  UINT32 max_mode = gop->Mode->MaxMode;
  Print(L"Current mode: %d, max mode: %d\n", gop->Mode->Mode, max_mode);

  // uefi_call_wrapper(gop->SetMode, 2, gop, max_mode-2);

  // Wait for keypress to give us time to attach a debugger, etc.
  // Print(L"Waiting for keypress to continue booting...\n");
  // wait_for_keypress();

  // Get memory map
  UINTN mem_map_size = 0, mem_map_key = 0, mem_map_descriptor_size = 0;
  uint8_t *mem_map = NULL;

  // Try to exit boot services 3 times
  for (int retries = 0; retries < 3; ++retries) {
    status = get_memory_map((void **)&mem_map, &mem_map_size, &mem_map_key,
                            &mem_map_descriptor_size);

    if (EFI_ERROR(status)) {
      Print(L"Error getting memory map!\n");
      return status;
    }

    //Try to load protocol infomation
    //EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *EntryArray;
    //status = uefi_call_wrapper(BS->OpenProtocolInformation, 4, HandleBuffer[0], &gop_guid, &EntryArray, NULL);
    //if (EFI_ERROR(status)) {
    //  Print(L"Error getting infomation of gop!\n");
    //  return status;
    //}
    
    //status = uefi_call_wrapper(BS->CloseProtocol, 4, HandleBuffer[0], &gop_guid, EntryArray->AgentHandle, EntryArray->ControllerHandle);
    //if (EFI_ERROR(status)) {
    //  Print(L"Error closing gop!\n");
    //  return status;
    //}
    // Exit boot services
    status =
        uefi_call_wrapper(BS->ExitBootServices, 2, ImageHandle, mem_map_key);
    // Execute kernel if we are successful
    if (status == EFI_SUCCESS) {
      // Jump to kernel code
      KernelInfo info = {.xdsp_address = xdsp_address,
                         .memory_map = mem_map,
                         .mem_map_size = mem_map_size,
                         .mem_map_descriptor_size = mem_map_descriptor_size,
                         .gop = gop};
      ((KernelMainFunc)kernel_main_addr)(info);
    }
  }

  /*
    If we've reached here, we haven't managed to exit boot services,
    we should print an error, free our resources and return.
  */

  Print(L"Unable to successfully exit boot services. Last status: %d\n",
        status);
  uefi_call_wrapper(BS->FreePool, 1, mem_map);

  return EFI_ABORTED;
}