/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <Protocol/EfiShellParameters.h>
#include <Library/BaseMemoryLib.h>
#include "DumpSupportCommand.h"
#include "NvmDimmCli.h"
#include "NvmInterface.h"
#include "LoadCommand.h"
#include "Debug.h"
#include "Convert.h"
#include <stdio.h>

extern EFI_SHELL_PARAMETERS_PROTOCOL gOsShellParametersProtocol;

/**
  Get FW debug log syntax definition
**/
struct Command DumpSupportCommandSyntax = {
  DUMP_VERB,                                                      //!< verb
  {                                                                 //!< options
    {L"", DESTINATION_OPTION, L"", DESTINATION_OPTION_HELP, TRUE, ValueRequired}
#ifdef OS_BUILD
    ,{ OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP, FALSE, ValueRequired }
#endif
  },
  {                                                                 //!< targets
    {SUPPORT_TARGET, L"", L"", TRUE, ValueEmpty}
  },
  {                                                                 //!< properties
    { L"", L"", L"", FALSE, ValueOptional }
  },
  L"Capture a snapshot of the system state for support purposes",   //!< help
  DumpSupportCommand,                                                //!< run function
  TRUE
};

typedef struct _DUMP_SUPPORT_CMD
{
  CHAR16 cmd[100];
} DUMP_SUPPORT_CMD;

#define MAX_CMDS 8

DUMP_SUPPORT_CMD DumpCmds[MAX_CMDS] = {
{L"version" },
{L"show -memoryresources"},
{L"show -a -dimm"},
{L"show -a -system -capabilities"},
{L"show -a -topology" },
{L"show -a -sensor"},
{L"start -diagnostic"},
{L"show -event"} };

#define NEW_DUMP_ENTRY_HEADER L"/*\n* %ls\n*/\n"

/**
  Register syntax of create -support
**/
EFI_STATUS
RegisterDumpSupportCommand(
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  NVDIMM_ENTRY();

  ReturnCode = RegisterCommand(&DumpSupportCommandSyntax);

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Dump support command

  @param[in] pCmd Command from CLI

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER pCmd NULL or invalid command line parameters
  @retval EFI_OUT_OF_RESOURCES Memory allocation failure
  @retval EFI_ABORTED invoking CONFIG_PROTOCOL Function failure
**/
EFI_STATUS
DumpSupportCommand(
  IN    struct Command *pCmd
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  COMMAND_STATUS *pCommandStatus = NULL;
  CHAR16 *pDumpUserPath = NULL;
  UINT32 Index = 0;
  struct CommandInput Input;
  struct Command Command;
  CHAR8 *pDumpUserPathAscii = NULL;
  FILE *hFile = NULL;
  PRINT_CONTEXT *pPrinterCtx = NULL;

  NVDIMM_ENTRY();

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    NVDIMM_DBG("pCmd parameter is NULL.\n");
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  pPrinterCtx = pCmd->pPrintCtx;

  /* Check -destination option */
  if (containsOption(pCmd, DESTINATION_OPTION)) {
    pDumpUserPath = getOptionValue(pCmd, DESTINATION_OPTION);
    if (pDumpUserPath == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      NVDIMM_ERR("Could not get -destination value. Out of memory.");
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OUT_OF_MEMORY);
      goto Finish;
    }
  }
  else {
    ReturnCode = EFI_INVALID_PARAMETER;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_PARSER_ERR_INVALID_OPTION_VALUES);
    goto Finish;
  }

  if(NULL == (pDumpUserPathAscii = AllocatePool((StrLen(pDumpUserPath) + 1) * sizeof(CHAR8))))
  {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OUT_OF_MEMORY);
    goto Finish;
  }

  UnicodeStrToAsciiStr(pDumpUserPath, pDumpUserPathAscii);
  if(NULL == (hFile = fopen(pDumpUserPathAscii, "w+")))
  {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OUT_OF_MEMORY);
    goto Finish;
  }
  gOsShellParametersProtocol.StdOut = (SHELL_FILE_HANDLE) hFile;

  for(Index = 0; Index < MAX_CMDS; ++Index)
  {
    Print(NEW_DUMP_ENTRY_HEADER, DumpCmds[Index].cmd);
    FillCommandInput(DumpCmds[Index].cmd, &Input);
    ReturnCode = Parse(&Input, &Command);

    if (!EFI_ERROR(ReturnCode)) {
      /* parse success, now run the command */
      ReturnCode = ExecuteCmd(&Command);
    }

    FreeCommandInput(&Input);
  }

  fclose(gOsShellParametersProtocol.StdOut);
  gOsShellParametersProtocol.StdOut = stdout;

  PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_INFO_DUMP_SUPPORT_SUCCESS L"\n", pDumpUserPath);

Finish:
  PRINTER_PROCESS_SET_BUFFER(pPrinterCtx);
  FreeCommandStatus(&pCommandStatus);
  FREE_POOL_SAFE(pDumpUserPath);
  FREE_POOL_SAFE(pDumpUserPathAscii);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
