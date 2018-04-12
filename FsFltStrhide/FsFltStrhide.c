/*++

Module Name:

    FsFltStrhide.c

Abstract:

    This is the main module of the FsFltStrhide miniFilter driver.

Environment:

    Kernel mode

--*/

#include <fltKernel.h>
#include <dontuse.h>
#include <suppress.h>

#pragma prefast(disable:__WARNING_ENCODE_MEMBER_FUNCTION_POINTER, "Not valid for kernel mode drivers")


PFLT_FILTER gFilterHandle;
ULONG_PTR OperationStatusCtx = 1;

#define PTDBG_TRACE_ROUTINES            0x00000001
#define PTDBG_TRACE_OPERATION_STATUS    0x00000002
#define PTDBG_TRACE_ERROR               0x00000004

ULONG gTraceFlags = 0x7;


#define PT_DBG_PRINT( _dbgLevel, _string )          \
    (FlagOn(gTraceFlags,(_dbgLevel)) ?              \
        DbgPrint _string :                          \
        ((int)0))

#define PT_DBG_PRINTEX( _ctDbgLevel, _dpfltDbgLevel, _fstring, ... ) \
    (FlagOn(gTraceFlags,(_ctDbgLevel)) ? \
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, (_dpfltDbgLevel), (_fstring), __VA_ARGS__ ) : \
        ((int)0))


/*************************************************************************
Pool Tags
*************************************************************************/

#define VOLCONTEXT_TAG      'xcBS'
#define FILECONTEXT_TAG     'flSH'
#define NAME_TAG            'mnBS'

/*************************************************************************
Local structures
*************************************************************************/

//
//  This is a volume context, one of these are attached to each volume
//  we monitor.  This is used to get a "DOS" name for debug display.
//

typedef struct _VOLUME_CONTEXT {

    //
    //  Holds the name to display
    //

    UNICODE_STRING Name;

    //
    //  Holds the sector size for this volume.
    //

    ULONG SectorSize;

} VOLUME_CONTEXT, *PVOLUME_CONTEXT;

#define MIN_SECTOR_SIZE 0x200


//
//  File context data structure
//

typedef struct _FILE_CONTEXT {

    //
    //  Name of the file associated with this context.
    //

    UNICODE_STRING FileName;

    //
    //  There is no resource to protect the context since the
    //  filename in the context is never modified. The filename
    //  is put in when the context is created and then freed
    //  with context is cleaned-up
    //

} FILE_CONTEXT, *PFILE_CONTEXT;

/*************************************************************************
    Prototypes
*************************************************************************/

EXTERN_C_START

DRIVER_INITIALIZE DriverEntry;
NTSTATUS
DriverEntry (
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    );

NTSTATUS
FsFltStrhideInstanceSetup (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_SETUP_FLAGS Flags,
    _In_ DEVICE_TYPE VolumeDeviceType,
    _In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
    );

VOID
CleanupContext(
    _In_ PFLT_CONTEXT Context,
    _In_ FLT_CONTEXT_TYPE ContextType
    );

VOID
FsFltStrhideInstanceTeardownStart (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
    );

VOID
FsFltStrhideInstanceTeardownComplete (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
    );

NTSTATUS
FsFltStrhideUnload (
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
    );

NTSTATUS
FsFltStrhideInstanceQueryTeardown (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
    );

FLT_PREOP_CALLBACK_STATUS
StrhidePreRead(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
    );

FLT_PREOP_CALLBACK_STATUS
FsFltStrhidePreOperation (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
    );

VOID
FsFltStrhideOperationStatusCallback (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ PFLT_IO_PARAMETER_BLOCK ParameterSnapshot,
    _In_ NTSTATUS OperationStatus,
    _In_ PVOID RequesterContext
    );

FLT_POSTOP_CALLBACK_STATUS
FsFltStrhidePostOperation (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
    );

FLT_PREOP_CALLBACK_STATUS
FsFltStrhidePreOperationNoPostOperation (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
    );

BOOLEAN
FsFltStrhideDoRequestOperationStatus(
    _In_ PFLT_CALLBACK_DATA Data
    );

EXTERN_C_END

//
//  Assign text sections for each routine.
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, FsFltStrhideUnload)
#pragma alloc_text(PAGE, FsFltStrhideInstanceQueryTeardown)
#pragma alloc_text(PAGE, FsFltStrhideInstanceSetup)
#pragma alloc_text(PAGE, CleanupContext)
#pragma alloc_text(PAGE, FsFltStrhideInstanceTeardownStart)
#pragma alloc_text(PAGE, FsFltStrhideInstanceTeardownComplete)
#pragma alloc_text(PAGE, StrhidePreRead)
#endif

//
//  operation registration
//

CONST FLT_OPERATION_REGISTRATION Callbacks[] = {

#if 0 // TODO - List all of the requests to filter.
    { IRP_MJ_CREATE,
      0,
      FsFltStrhidePreOperation,
      FsFltStrhidePostOperation },

    { IRP_MJ_CREATE_NAMED_PIPE,
      0,
      FsFltStrhidePreOperation,
      FsFltStrhidePostOperation },

    { IRP_MJ_CLOSE,
      0,
      FsFltStrhidePreOperation,
      FsFltStrhidePostOperation },
#endif
    { IRP_MJ_READ,
      0,
      StrhidePreRead,
      NULL }, //optional
#if 0
    { IRP_MJ_WRITE,
      0,
      FsFltStrhidePreOperation,
      FsFltStrhidePostOperation },

    { IRP_MJ_QUERY_INFORMATION,
      0,
      FsFltStrhidePreOperation,
      FsFltStrhidePostOperation },

    { IRP_MJ_SET_INFORMATION,
      0,
      FsFltStrhidePreOperation,
      FsFltStrhidePostOperation },

    { IRP_MJ_QUERY_EA,
      0,
      FsFltStrhidePreOperation,
      FsFltStrhidePostOperation },

    { IRP_MJ_SET_EA,
      0,
      FsFltStrhidePreOperation,
      FsFltStrhidePostOperation },

    { IRP_MJ_FLUSH_BUFFERS,
      0,
      FsFltStrhidePreOperation,
      FsFltStrhidePostOperation },

    { IRP_MJ_QUERY_VOLUME_INFORMATION,
      0,
      FsFltStrhidePreOperation,
      FsFltStrhidePostOperation },

    { IRP_MJ_SET_VOLUME_INFORMATION,
      0,
      FsFltStrhidePreOperation,
      FsFltStrhidePostOperation },

    { IRP_MJ_DIRECTORY_CONTROL,
      0,
      FsFltStrhidePreOperation,
      FsFltStrhidePostOperation },

    { IRP_MJ_FILE_SYSTEM_CONTROL,
      0,
      FsFltStrhidePreOperation,
      FsFltStrhidePostOperation },

    { IRP_MJ_DEVICE_CONTROL,
      0,
      FsFltStrhidePreOperation,
      FsFltStrhidePostOperation },

    { IRP_MJ_INTERNAL_DEVICE_CONTROL,
      0,
      FsFltStrhidePreOperation,
      FsFltStrhidePostOperation },

    { IRP_MJ_SHUTDOWN,
      0,
      FsFltStrhidePreOperationNoPostOperation,
      NULL },                               //post operations not supported

    { IRP_MJ_LOCK_CONTROL,
      0,
      FsFltStrhidePreOperation,
      FsFltStrhidePostOperation },

    { IRP_MJ_CLEANUP,
      0,
      FsFltStrhidePreOperation,
      FsFltStrhidePostOperation },

    { IRP_MJ_CREATE_MAILSLOT,
      0,
      FsFltStrhidePreOperation,
      FsFltStrhidePostOperation },

    { IRP_MJ_QUERY_SECURITY,
      0,
      FsFltStrhidePreOperation,
      FsFltStrhidePostOperation },

    { IRP_MJ_SET_SECURITY,
      0,
      FsFltStrhidePreOperation,
      FsFltStrhidePostOperation },

    { IRP_MJ_QUERY_QUOTA,
      0,
      FsFltStrhidePreOperation,
      FsFltStrhidePostOperation },

    { IRP_MJ_SET_QUOTA,
      0,
      FsFltStrhidePreOperation,
      FsFltStrhidePostOperation },

    { IRP_MJ_PNP,
      0,
      FsFltStrhidePreOperation,
      FsFltStrhidePostOperation },

    { IRP_MJ_ACQUIRE_FOR_SECTION_SYNCHRONIZATION,
      0,
      FsFltStrhidePreOperation,
      FsFltStrhidePostOperation },

    { IRP_MJ_RELEASE_FOR_SECTION_SYNCHRONIZATION,
      0,
      FsFltStrhidePreOperation,
      FsFltStrhidePostOperation },

    { IRP_MJ_ACQUIRE_FOR_MOD_WRITE,
      0,
      FsFltStrhidePreOperation,
      FsFltStrhidePostOperation },

    { IRP_MJ_RELEASE_FOR_MOD_WRITE,
      0,
      FsFltStrhidePreOperation,
      FsFltStrhidePostOperation },

    { IRP_MJ_ACQUIRE_FOR_CC_FLUSH,
      0,
      FsFltStrhidePreOperation,
      FsFltStrhidePostOperation },

    { IRP_MJ_RELEASE_FOR_CC_FLUSH,
      0,
      FsFltStrhidePreOperation,
      FsFltStrhidePostOperation },

    { IRP_MJ_FAST_IO_CHECK_IF_POSSIBLE,
      0,
      FsFltStrhidePreOperation,
      FsFltStrhidePostOperation },

    { IRP_MJ_NETWORK_QUERY_OPEN,
      0,
      FsFltStrhidePreOperation,
      FsFltStrhidePostOperation },

    { IRP_MJ_MDL_READ,
      0,
      FsFltStrhidePreOperation,
      FsFltStrhidePostOperation },

    { IRP_MJ_MDL_READ_COMPLETE,
      0,
      FsFltStrhidePreOperation,
      FsFltStrhidePostOperation },

    { IRP_MJ_PREPARE_MDL_WRITE,
      0,
      FsFltStrhidePreOperation,
      FsFltStrhidePostOperation },

    { IRP_MJ_MDL_WRITE_COMPLETE,
      0,
      FsFltStrhidePreOperation,
      FsFltStrhidePostOperation },

    { IRP_MJ_VOLUME_MOUNT,
      0,
      FsFltStrhidePreOperation,
      FsFltStrhidePostOperation },

    { IRP_MJ_VOLUME_DISMOUNT,
      0,
      FsFltStrhidePreOperation,
      FsFltStrhidePostOperation },

#endif // TODO

    { IRP_MJ_OPERATION_END }
};

//
//  Context definitions we currently care about.  Note that the system will
//  create a lookAside list for the volume context because an explicit size
//  of the context is specified.
//

CONST FLT_CONTEXT_REGISTRATION ContextNotifications[] = {

     { FLT_VOLUME_CONTEXT,
       0,
       CleanupContext,
       sizeof(VOLUME_CONTEXT),
       VOLCONTEXT_TAG },

     { FLT_FILE_CONTEXT,
       0,
       CleanupContext,
       sizeof(FILE_CONTEXT),
       FILECONTEXT_TAG },

     { FLT_CONTEXT_END }
};

//
//  This defines what we want to filter with FltMgr
//

CONST FLT_REGISTRATION FilterRegistration = {

    sizeof( FLT_REGISTRATION ),         //  Size
    FLT_REGISTRATION_VERSION,           //  Version
    0,                                  //  Flags

    ContextNotifications,               //  Context
    Callbacks,                          //  Operation callbacks

    FsFltStrhideUnload,                           //  MiniFilterUnload

    FsFltStrhideInstanceSetup,                    //  InstanceSetup
    FsFltStrhideInstanceQueryTeardown,            //  InstanceQueryTeardown
    FsFltStrhideInstanceTeardownStart,            //  InstanceTeardownStart
    FsFltStrhideInstanceTeardownComplete,         //  InstanceTeardownComplete

    NULL,                               //  GenerateFileName
    NULL,                               //  GenerateDestinationFileName
    NULL                                //  NormalizeNameComponent

};



NTSTATUS
FsFltStrhideInstanceSetup (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_SETUP_FLAGS Flags,
    _In_ DEVICE_TYPE VolumeDeviceType,
    _In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
    )
/*++

Routine Description:

    This routine is called whenever a new instance is created on a volume.

    By default we want to attach to all volumes.  This routine will try and
    get a "DOS" name for the given volume.  If it can't, it will try and
    get the "NT" name for the volume (which is what happens on network
    volumes).  If a name is retrieved a volume context will be created with
    that name.

Arguments:

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance and its associated volume.

    Flags - Flags describing the reason for this attach request.

Return Value:

    STATUS_SUCCESS - attach
    STATUS_FLT_DO_NOT_ATTACH - do not attach

--*/
{
    PDEVICE_OBJECT devObj = NULL;
    PVOLUME_CONTEXT volctx = NULL;
    PFILE_CONTEXT filctx = NULL;
    PFLT_FILE_NAME_INFORMATION filenameInfo = NULL;
    NTSTATUS status = STATUS_SUCCESS;
    ULONG retLen;
    PUNICODE_STRING workingName;
    USHORT size;
    UCHAR volPropBuffer[sizeof(FLT_VOLUME_PROPERTIES) + 512];
    PFLT_VOLUME_PROPERTIES volProp = (PFLT_VOLUME_PROPERTIES)volPropBuffer;

    UNREFERENCED_PARAMETER( Flags );
    UNREFERENCED_PARAMETER( VolumeDeviceType );
    UNREFERENCED_PARAMETER( VolumeFilesystemType );

    PAGED_CODE();

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("FsFltStrhide!FsFltStrhideInstanceSetup: Entered\n") );

    try {

#pragma region VolumeContextInit

        //
        //  Allocate a volume context structure.
        //

        status = FltAllocateContext( FltObjects->Filter,
                                     FLT_VOLUME_CONTEXT,
                                     sizeof(VOLUME_CONTEXT),
                                     NonPagedPool,
                                     &volctx );

        if (!NT_SUCCESS(status)) {

            //
            //  We could not allocate a context, quit now
            //

            PT_DBG_PRINTEX( PTDBG_TRACE_ERROR, DPFLTR_ERROR_LEVEL,
                "FsFltStrhide!FsFltStrhideInstanceSetup: Failed to create volume context with status 0x%x. (FltObjects @ %p)\n",
                status,
                FltObjects );

            leave;
        }

        //
        //  Always get the volume properties, so I can get a sector size
        //

        status = FltGetVolumeProperties( FltObjects->Volume,
                                         volProp,
                                         sizeof(volPropBuffer),
                                         &retLen );

        if (!NT_SUCCESS(status)) {

            leave;
        }

        //
        //  Save the sector size in the context for later use.  Note that
        //  we will pick a minimum sector size if a sector size is not
        //  specified.
        //

        FLT_ASSERT((volProp->SectorSize == 0) || (volProp->SectorSize >= MIN_SECTOR_SIZE));

        volctx->SectorSize = max(volProp->SectorSize,MIN_SECTOR_SIZE);

        //
        //  Init the buffer field (which may be allocated later).
        //

        volctx->Name.Buffer = NULL;

        //
        //  Get the storage device object we want a name for.
        //

        status = FltGetDiskDeviceObject( FltObjects->Volume, &devObj );

        if (NT_SUCCESS(status)) {

            //
            //  Try and get the DOS name.  If it succeeds we will have
            //  an allocated name buffer.  If not, it will be NULL
            //

            status = IoVolumeDeviceToDosName( devObj, &volctx->Name );
        }

        //
        //  If we could not get a DOS name, get the NT name.
        //

        if (!NT_SUCCESS(status)) {

            FLT_ASSERT(volctx->Name.Buffer == NULL);

            //
            //  Figure out which name to use from the properties
            //

            if (volProp->RealDeviceName.Length > 0) {

                workingName = &volProp->RealDeviceName;

            } else if (volProp->FileSystemDeviceName.Length > 0) {

                workingName = &volProp->FileSystemDeviceName;

            } else {

                //
                //  No name, don't save the context
                //

                status = STATUS_FLT_DO_NOT_ATTACH;
                leave;
            }

            //
            //  Get size of buffer to allocate.  This is the length of the
            //  string plus room for a trailing colon.
            //

            size = workingName->Length + sizeof(WCHAR);

            //
            //  Now allocate a buffer to hold this name
            //

#pragma prefast(suppress:__WARNING_MEMORY_LEAK, "volctx->Name.Buffer will not be leaked because it is freed in CleanupContext")
            volctx->Name.Buffer = ExAllocatePoolWithTag( NonPagedPool,
                                                      size,
                                                      NAME_TAG );
            if (volctx->Name.Buffer == NULL) {

                status = STATUS_INSUFFICIENT_RESOURCES;
                leave;
            }

            //
            //  Init the rest of the fields
            //

            volctx->Name.Length = 0;
            volctx->Name.MaximumLength = size;

            //
            //  Copy the name in
            //

            RtlCopyUnicodeString( &volctx->Name,
                                  workingName );

            //
            //  Put a trailing colon to make the display look good
            //

            RtlAppendUnicodeToString( &volctx->Name,
                                      L":" );
        }

        //
        //  Set the context
        //

        status = FltSetVolumeContext( FltObjects->Volume,
                                      FLT_SET_CONTEXT_KEEP_IF_EXISTS,
                                      volctx,
                                      NULL );

        //
        //  Log debug info
        //

        PT_DBG_PRINTEX( PTDBG_TRACE_OPERATION_STATUS, DPFLTR_TRACE_LEVEL,
            "FsFltStrhide!FsFltStrhideInstanceSetup:                  Real SectSize=0x%04x, Used SectSize=0x%04x, Name=\"%wZ\"\n",
                    volProp->SectorSize,
                    volctx->SectorSize,
                    &volctx->Name);

        //
        //  It is OK for the context to already be defined.
        //

        if (status == STATUS_FLT_CONTEXT_ALREADY_DEFINED) {

            status = STATUS_SUCCESS;
        }
        else if (!NT_SUCCESS(status)) {

            // h e r e   w e   g o
            // (I think I'll just go ahead and try to make a file context)
            PT_DBG_PRINTEX( PTDBG_TRACE_ERROR, DPFLTR_ERROR_LEVEL,
                "FsFltStrhide!FsFltStrhideInstanceSetup: Failed to set volume context with volume %wZ\n",
                &volctx->Name);
        }

#pragma endregion

#pragma region FileContextInit

        //
        //  Allocate a file context structure.
        //

        status = FltAllocateContext( FltObjects->Filter,
                                     FLT_FILE_CONTEXT,
                                     sizeof(FILE_CONTEXT),
                                     NonPagedPool,
                                     &filctx );

        if (!NT_SUCCESS(status)) {

            //
            //  We could not allocate a context, quit now
            //

            PT_DBG_PRINTEX( PTDBG_TRACE_ERROR, DPFLTR_ERROR_LEVEL,
                "FsFltStrhide!FsFltStrhideInstanceSetup: Failed to create file context with status 0x%x. (FltObjects @ %p)\n",
                status,
                FltObjects );

            leave;
        }

        status = FltGetFileNameInformationUnsafe( FltObjects->FileObject,
                                                  FltObjects->Instance,
                                                  FLT_FILE_NAME_NORMALIZED |
                                                  FLT_FILE_NAME_QUERY_DEFAULT,
                                                  &filenameInfo );

#pragma endregion

    } finally {

        if (filenameInfo) {

            FltReleaseFileNameInformation( filenameInfo );
        }

        //
        //  Always release the contexts.  If the sets failed, it will free the
        //  contexts.  If not, it will remove the references added by the sets.
        //  Note that the UNICODE_STRING buffers in the contexts will get freed
        //  by the context cleanup routine.
        //

        if (volctx) {

            FltReleaseContext( volctx );
        }

        if (filctx) {

            FltReleaseContext( filctx );
        }

        //
        //  Remove the reference added to the device object by
        //  FltGetDiskDeviceObject.
        //

        if (devObj) {

            ObDereferenceObject( devObj );
        }
    }

    return status;
}


VOID
CleanupContext(
    _In_ PFLT_CONTEXT Context,
    _In_ FLT_CONTEXT_TYPE ContextType
    )
/*++

Routine Description:

    The given context is being freed.
    Free allocated string buffers if applicable.

Arguments:

    Context - The context being freed

    ContextType - The type of context this is

Return Value:

    None

--*/
{
    PAGED_CODE();

    FLT_ASSERT(ContextType == FLT_VOLUME_CONTEXT
            || ContextType == FLT_FILE_CONTEXT);

    if (ContextType == FLT_VOLUME_CONTEXT) {
        PVOLUME_CONTEXT ctx = Context;

        // Free the allocated name buffer if there is one.
        if (ctx->Name.Buffer != NULL) {

            ExFreePool(ctx->Name.Buffer);
            ctx->Name.Buffer = NULL;
        }
    } else { // ContextType == FLT_FILE_CONTEXT
        PFILE_CONTEXT ctx = Context;

        // Free the allocated name buffer if there is one.
        if (ctx->FileName.Buffer != NULL) {

            ExFreePool(ctx->FileName.Buffer);
            ctx->FileName.Buffer = NULL;
        }
    }
}


NTSTATUS
FsFltStrhideInstanceQueryTeardown (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
    )
/*++

Routine Description:

    This is called when an instance is being manually deleted by a
    call to FltDetachVolume or FilterDetach thereby giving us a
    chance to fail that detach request.

    If this routine is not defined in the registration structure, explicit
    detach requests via FltDetachVolume or FilterDetach will always be
    failed.

Arguments:

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance and its associated volume.

    Flags - Indicating where this detach request came from.

Return Value:

    Returns the status of this operation.

--*/
{
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("FsFltStrhide!FsFltStrhideInstanceQueryTeardown: Entered\n") );

    return STATUS_SUCCESS;
}


VOID
FsFltStrhideInstanceTeardownStart (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
    )
/*++

Routine Description:

    This routine is called at the start of instance teardown.

Arguments:

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance and its associated volume.

    Flags - Reason why this instance is being deleted.

Return Value:

    None.

--*/
{
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("FsFltStrhide!FsFltStrhideInstanceTeardownStart: Entered\n") );
}


VOID
FsFltStrhideInstanceTeardownComplete (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
    )
/*++

Routine Description:

    This routine is called at the end of instance teardown.

Arguments:

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance and its associated volume.

    Flags - Reason why this instance is being deleted.

Return Value:

    None.

--*/
{
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("FsFltStrhide!FsFltStrhideInstanceTeardownComplete: Entered\n") );
}


/*************************************************************************
    MiniFilter initialization and unload routines.
*************************************************************************/

NTSTATUS
DriverEntry (
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
/*++

Routine Description:

    This is the initialization routine for this miniFilter driver.  This
    registers with FltMgr and initializes all global data structures.

Arguments:

    DriverObject - Pointer to driver object created by the system to
        represent this driver.

    RegistryPath - Unicode string identifying where the parameters for this
        driver are located in the registry.

Return Value:

    Routine can return non success error codes.

--*/
{
    NTSTATUS status;

    UNREFERENCED_PARAMETER( RegistryPath );

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("FsFltStrhide!DriverEntry: Entered\n") );

    //
    //  Register with FltMgr to tell it our callback routines
    //

    status = FltRegisterFilter( DriverObject,
                                &FilterRegistration,
                                &gFilterHandle );

    FLT_ASSERT( NT_SUCCESS( status ) );

    if (NT_SUCCESS( status )) {

        //
        //  Start filtering i/o
        //

        status = FltStartFiltering( gFilterHandle );

        if (!NT_SUCCESS( status )) {

            FltUnregisterFilter( gFilterHandle );
        }
    }

    return status;
}

NTSTATUS
FsFltStrhideUnload (
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
    )
/*++

Routine Description:

    This is the unload routine for this miniFilter driver. This is called
    when the minifilter is about to be unloaded. We can fail this unload
    request if this is not a mandatory unload indicated by the Flags
    parameter.

Arguments:

    Flags - Indicating if this is a mandatory unload.

Return Value:

    Returns STATUS_SUCCESS.

--*/
{
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("FsFltStrhide!FsFltStrhideUnload: Entered\n") );

    FltUnregisterFilter( gFilterHandle );

    return STATUS_SUCCESS;
}


/*************************************************************************
    MiniFilter callback routines.
*************************************************************************/
FLT_PREOP_CALLBACK_STATUS
StrhidePreRead(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
    )
/*++
Routine Description:

    This routine detects the presence of the target byte sequence
    and prints debug output on where it has been detected.

Arguments:

    Data - Pointer to the filter callbackData that is passed to us.

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance, its associated volume and
        file object.

    CompletionContext - Receives the context that will be passed to the
        post-operation callback.

Return Value:

    FLT_PREOP_SUCCESS_WITH_CALLBACK - we want a postOperation callback

    FLT_PREOP_SUCCESS_NO_CALLBACK - we don't want a postOperation callback

--*/
{
    PFLT_IO_PARAMETER_BLOCK iopb = Data->Iopb;
    FLT_PREOP_CALLBACK_STATUS retValue = FLT_PREOP_SUCCESS_NO_CALLBACK;
    PVOLUME_CONTEXT volCtx = NULL;
    PFILE_CONTEXT filCtx = NULL;
    PFLT_FILE_NAME_INFORMATION filenameInfo = NULL;
    //UNICODE_STRING filename; //UNREFERENCED

    BOOLEAN isFileContextSupported = FALSE;
    NTSTATUS status;
    ULONG readLen = iopb->Parameters.Read.Length;
    PVOID readbufAddr = iopb->Parameters.Read.ReadBuffer;

    UNREFERENCED_PARAMETER(CompletionContext);

    try {

        //
        //  If they are trying to read ZERO bytes, then don't do anything and
        //  we don't need a post-operation callback.
        //

        if (readLen == 0) {

            leave;
        }

        //
        //  Get file name
        //

        status = FltGetFileNameInformation( Data,
                                            FLT_FILE_NAME_NORMALIZED |
                                            FLT_FILE_NAME_QUERY_DEFAULT,
                                            &filenameInfo );

        if (NT_SUCCESS( status )) {

            status = FltParseFileNameInformation( filenameInfo );

            if (NT_SUCCESS( status )) {
                const static UNICODE_STRING nullstr = RTL_CONSTANT_STRING( L"(null)" );

                PT_DBG_PRINTEX( PTDBG_TRACE_OPERATION_STATUS, DPFLTR_INFO_LEVEL,
                    "FsFltStrhide!StrhidePreRead: Obtained file name information:\n"
                        "\t Name      : %wZ\n"
                        "\t Volume    : %wZ\n"
                        "\t Share     : %wZ\n"
                        "\t Extension : %wZ\n"
                        "\t Stream    : %wZ\n"
                        "\t Fin.Comp  : %wZ\n"
                        "\t ParentDir : %wZ\n",
                    &filenameInfo->Name,
                    &filenameInfo->Volume ? &filenameInfo->Volume : &nullstr,
                    &filenameInfo->Share ? &filenameInfo->Share : &nullstr,
                    &filenameInfo->Extension ? &filenameInfo->Extension : &nullstr,
                    &filenameInfo->Stream ? &filenameInfo->Stream : &nullstr,
                    &filenameInfo->FinalComponent ? &filenameInfo->FinalComponent : &nullstr,
                    &filenameInfo->ParentDir ? &filenameInfo->ParentDir : &nullstr );
            }
            else { // Failed to parse file information

                PT_DBG_PRINTEX( PTDBG_TRACE_OPERATION_STATUS, DPFLTR_INFO_LEVEL | DPFLTR_WARNING_LEVEL,
                    "FsFltStrhide!StrhidePreRead: Obtained file name information (parse failed):\n"
                        "\t Name      : %wZ\n",
                    &filenameInfo->Name );
            }
        }
        else {

            PT_DBG_PRINTEX( PTDBG_TRACE_ERROR, DPFLTR_ERROR_LEVEL,
                "FsFltStrhide!StrhidePreRead: Failed to get file name information (CallbackData = %p, FileObject = %p)\n",
                Data,
                FltObjects->FileObject );

            leave;
        }


        //
        //  (Try to) Get our file context so we can display the file name
        //  in the debug output.
        //

        isFileContextSupported = 
            FltSupportsFileContextsEx( FltObjects->FileObject,
                                       FltObjects->Instance );

        if (isFileContextSupported) {

            status = FltGetVolumeContext( FltObjects->Filter,
                                          FltObjects->Volume,
                                          &volCtx );

            if (!NT_SUCCESS(status)) {

                PT_DBG_PRINTEX( PTDBG_TRACE_ERROR, DPFLTR_ERROR_LEVEL,
                    "FsFltStrhide!StrhidePreRead: Error getting volume context, status=%x\n",
                    status);

                leave;
            }
        }

        //
        //  Get our file context so we can display.. stuff. //TODO UPDATE
        //

        status = FltGetFileContext( FltObjects->Instance,
                                    FltObjects->FileObject,
                                    &filCtx );

        if (!NT_SUCCESS(status)) {

            PT_DBG_PRINTEX( PTDBG_TRACE_ERROR, DPFLTR_ERROR_LEVEL,
                "FsFltStrhide!StrhidePreRead: Error getting file context, status=%x\n",
                status);

            leave;
        }

        PT_DBG_PRINTEX( PTDBG_TRACE_OPERATION_STATUS, DPFLTR_TRACE_LEVEL,
            "FsFltStrhide!StrhidePreRead: [VOLUME_CONTEXT] %wZ bufaddr=%p mdladdr=%p len=%d\n",
            &volCtx->Name,
            iopb->Parameters.Read.ReadBuffer,
            iopb->Parameters.Read.MdlAddress,
            readLen);

        PT_DBG_PRINTEX( PTDBG_TRACE_OPERATION_STATUS, DPFLTR_TRACE_LEVEL,
            "FsFltStrhide!StrhidePreRead: [FILE_CONTEXT] FileName=%wZ\n",
            &filCtx->FileName);

    } finally {

        //
        //  Cleanup state.
        //

        if (volCtx != NULL) {
            FltReleaseContext( volCtx );
        }

        if (filCtx != NULL) {
            FltReleaseContext( filCtx );
        }

        if (filenameInfo != NULL) {
            FltReleaseFileNameInformation( filenameInfo );
        }
    }

    return retValue;
}



FLT_PREOP_CALLBACK_STATUS
FsFltStrhidePreOperation (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
    )
/*++

Routine Description:

    This routine is a pre-operation dispatch routine for this miniFilter.

    This is non-pageable because it could be called on the paging path

Arguments:

    Data - Pointer to the filter callbackData that is passed to us.

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance, its associated volume and
        file object.

    CompletionContext - The context for the completion routine for this
        operation.

Return Value:

    The return value is the status of the operation.

--*/
{
    NTSTATUS status;

    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( CompletionContext );

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("FsFltStrhide!FsFltStrhidePreOperation: Entered\n") );

    //
    //  See if this is an operation we would like the operation status
    //  for.  If so request it.
    //
    //  NOTE: most filters do NOT need to do this.  You only need to make
    //        this call if, for example, you need to know if the oplock was
    //        actually granted.
    //

    if (FsFltStrhideDoRequestOperationStatus( Data )) {

        status = FltRequestOperationStatusCallback( Data,
                                                    FsFltStrhideOperationStatusCallback,
                                                    (PVOID)(++OperationStatusCtx) );
        if (!NT_SUCCESS(status)) {

            PT_DBG_PRINT( PTDBG_TRACE_OPERATION_STATUS,
                          ("FsFltStrhide!FsFltStrhidePreOperation: FltRequestOperationStatusCallback Failed, status=%08x\n",
                           status) );
        }
    }

    // This template code does not do anything with the callbackData, but
    // rather returns FLT_PREOP_SUCCESS_WITH_CALLBACK.
    // This passes the request down to the next miniFilter in the chain.

    return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}



VOID
FsFltStrhideOperationStatusCallback (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ PFLT_IO_PARAMETER_BLOCK ParameterSnapshot,
    _In_ NTSTATUS OperationStatus,
    _In_ PVOID RequesterContext
    )
/*++

Routine Description:

    This routine is called when the given operation returns from the call
    to IoCallDriver.  This is useful for operations where STATUS_PENDING
    means the operation was successfully queued.  This is useful for OpLocks
    and directory change notification operations.

    This callback is called in the context of the originating thread and will
    never be called at DPC level.  The file object has been correctly
    referenced so that you can access it.  It will be automatically
    dereferenced upon return.

    This is non-pageable because it could be called on the paging path

Arguments:

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance, its associated volume and
        file object.

    RequesterContext - The context for the completion routine for this
        operation.

    OperationStatus -

Return Value:

    The return value is the status of the operation.

--*/
{
    UNREFERENCED_PARAMETER( FltObjects );

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("FsFltStrhide!FsFltStrhideOperationStatusCallback: Entered\n") );

    PT_DBG_PRINT( PTDBG_TRACE_OPERATION_STATUS,
                  ("FsFltStrhide!FsFltStrhideOperationStatusCallback: Status=%08x ctx=%p IrpMj=%02x.%02x \"%s\"\n",
                   OperationStatus,
                   RequesterContext,
                   ParameterSnapshot->MajorFunction,
                   ParameterSnapshot->MinorFunction,
                   FltGetIrpName(ParameterSnapshot->MajorFunction)) );
}


FLT_POSTOP_CALLBACK_STATUS
FsFltStrhidePostOperation (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
    )
/*++

Routine Description:

    This routine is the post-operation completion routine for this
    miniFilter.

    This is non-pageable because it may be called at DPC level.

Arguments:

    Data - Pointer to the filter callbackData that is passed to us.

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance, its associated volume and
        file object.

    CompletionContext - The completion context set in the pre-operation routine.

    Flags - Denotes whether the completion is successful or is being drained.

Return Value:

    The return value is the status of the operation.

--*/
{
    UNREFERENCED_PARAMETER( Data );
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( CompletionContext );
    UNREFERENCED_PARAMETER( Flags );

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("FsFltStrhide!FsFltStrhidePostOperation: Entered\n") );

    return FLT_POSTOP_FINISHED_PROCESSING;
}


FLT_PREOP_CALLBACK_STATUS
FsFltStrhidePreOperationNoPostOperation (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
    )
/*++

Routine Description:

    This routine is a pre-operation dispatch routine for this miniFilter.

    This is non-pageable because it could be called on the paging path

Arguments:

    Data - Pointer to the filter callbackData that is passed to us.

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance, its associated volume and
        file object.

    CompletionContext - The context for the completion routine for this
        operation.

Return Value:

    The return value is the status of the operation.

--*/
{
    UNREFERENCED_PARAMETER( Data );
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( CompletionContext );

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("FsFltStrhide!FsFltStrhidePreOperationNoPostOperation: Entered\n") );

    // This template code does not do anything with the callbackData, but
    // rather returns FLT_PREOP_SUCCESS_NO_CALLBACK.
    // This passes the request down to the next miniFilter in the chain.

    return FLT_PREOP_SUCCESS_NO_CALLBACK;
}


BOOLEAN
FsFltStrhideDoRequestOperationStatus(
    _In_ PFLT_CALLBACK_DATA Data
    )
/*++

Routine Description:

    This identifies those operations we want the operation status for.  These
    are typically operations that return STATUS_PENDING as a normal completion
    status.

Arguments:

Return Value:

    TRUE - If we want the operation status
    FALSE - If we don't

--*/
{
    PFLT_IO_PARAMETER_BLOCK iopb = Data->Iopb;

    //
    //  return boolean state based on which operations we are interested in
    //

    return (BOOLEAN)

            //
            //  Check for oplock operations
            //

             (((iopb->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL) &&
               ((iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_REQUEST_FILTER_OPLOCK)  ||
                (iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_REQUEST_BATCH_OPLOCK)   ||
                (iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_REQUEST_OPLOCK_LEVEL_1) ||
                (iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_REQUEST_OPLOCK_LEVEL_2)))

              ||

              //
              //    Check for directy change notification
              //

              ((iopb->MajorFunction == IRP_MJ_DIRECTORY_CONTROL) &&
               (iopb->MinorFunction == IRP_MN_NOTIFY_CHANGE_DIRECTORY))
             );
}
