#include <Windows.h>
#include <libipt.h>
#include <ipt.h>

FORCEINLINE
VOID
InitializeIptBuffer (
    _Inout_ PIPT_INPUT_BUFFER pBuffer,
    _In_ IPT_INPUT_TYPE dwInputType
    )
{
    //
    // Zero it out and set the version
    //
    ZeroMemory(pBuffer, sizeof(*pBuffer));
    pBuffer->BufferMajorVersion = IPT_BUFFER_MAJOR_VERSION_CURRENT;
    pBuffer->BufferMinorVersion = IPT_BUFFER_MINOR_VERSION_CURRENT;

    //
    // Set the type
    //
    pBuffer->InputType = dwInputType;
}

FORCEINLINE
BOOL
OpenIptDevice (
    _Out_ PHANDLE phFile
    )
{
    //
    // Open the handle
    //
    *phFile = CreateFile(L"\\??\\IPT",
                         FILE_GENERIC_READ,
                         FILE_SHARE_READ,
                         NULL,
                         OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL |
                         FILE_FLAG_SEQUENTIAL_SCAN |
                         FILE_FLAG_NO_BUFFERING,
                         NULL);

    //
    // Return the result
    //
    return (*phFile == INVALID_HANDLE_VALUE) ? FALSE : TRUE;
}

BOOL
GetIptBufferVersion (
    _Out_ PDWORD pdwBufferMajorVersion
    )
{
    BOOL bRes;
    HANDLE iptHandle;
    IPT_INPUT_BUFFER inputBuffer;
    IPT_BUFFER_VERSION outputBuffer;

    //
    // Initialize for failure
    //
    *pdwBufferMajorVersion = 0;

    //
    // Open the IPT Device
    //
    bRes = OpenIptDevice(&iptHandle);
    if (bRes != FALSE)
    {
        //
        // Send only the version header of the input request.
        // The type is unused.
        //
        InitializeIptBuffer(&inputBuffer, -1);
        bRes = DeviceIoControl(iptHandle,
                               IOCTL_IPT_REQUEST,
                               &inputBuffer,
                               sizeof(IPT_BUFFER_VERSION),
                               &outputBuffer,
                               sizeof(outputBuffer),
                               NULL,
                               NULL);
        CloseHandle(iptHandle);

        //
        // On success, return the buffer version
        //
        if (bRes != FALSE)
        {
            *pdwBufferMajorVersion = outputBuffer.BufferMajorVersion;
        }
    }
    return bRes;
}

BOOL
GetIptTraceVersion (
    _Out_ PWORD pwTraceVersion
    )
{
    BOOL bRes;
    HANDLE iptHandle;
    IPT_INPUT_BUFFER inputBuffer;
    IPT_OUTPUT_BUFFER outputBuffer;

    //
    // Initialize for failure
    //
    *pwTraceVersion = 0;

    //
    // Open the IPT Device
    //
    bRes = OpenIptDevice(&iptHandle);
    if (bRes != FALSE)
    {
        //
        // Send a request to get the trace version
        //
        InitializeIptBuffer(&inputBuffer, IptGetTraceVersion);
        bRes = DeviceIoControl(iptHandle,
                               IOCTL_IPT_REQUEST,
                               &inputBuffer,
                               sizeof(inputBuffer),
                               &outputBuffer,
                               sizeof(outputBuffer),
                               NULL,
                               NULL);
        CloseHandle(iptHandle);

        //
        // On success, return the buffer version
        //
        if (bRes != FALSE)
        {
            *pwTraceVersion = outputBuffer.GetTraceVersion.TraceVersion;
        }
    }
    return bRes;
}

BOOL
GetProcessIptTraceSize (
    _In_ HANDLE hProcess,
    _Out_ PDWORD pdwTraceSize
    )
{
    BOOL bRes;
    HANDLE iptHandle;
    IPT_INPUT_BUFFER inputBuffer;
    IPT_OUTPUT_BUFFER outputBuffer;

    //
    // Initialize for failure
    //
    *pdwTraceSize = 0;

    //
    // Open the IPT Device
    //
    bRes = OpenIptDevice(&iptHandle);
    if (bRes != FALSE)
    {
        //
        // Send a request to get the trace size for the process
        //
        InitializeIptBuffer(&inputBuffer, IptGetProcessTraceSize);
        inputBuffer.GetProcessIptTraceSize.TraceVersion = IPT_TRACE_VERSION_CURRENT;
        inputBuffer.GetProcessIptTraceSize.ProcessHandle = hProcess;
        bRes = DeviceIoControl(iptHandle,
                               IOCTL_IPT_REQUEST,
                               &inputBuffer,
                               sizeof(inputBuffer),
                               &outputBuffer,
                               sizeof(outputBuffer),
                               NULL,
                               NULL);
        CloseHandle(iptHandle);

        //
        // Check if we got a size back
        //
        if (bRes != FALSE)
        {
            //
            // The IOCTL layer supports > 4GB traces but this doesn't exist yet
            // Otherwise, return the 32-bit trace size.
            //
            if (outputBuffer.GetTraceSize.TraceSize <= ULONG_MAX)
            {
                *pdwTraceSize = (DWORD)outputBuffer.GetTraceSize.TraceSize;
            }
            else
            {
                //
                // Mark this as a failure -- this is the Windows behavior too
                //
                SetLastError(ERROR_IMPLEMENTATION_LIMIT);
                bRes = FALSE;
            }
        }
    }
    return bRes;
}

BOOL
GetProcessIptTrace (
    _In_ HANDLE hProcess,
    _In_ PVOID pTrace,
    _In_ DWORD dwTraceSize
    )
{
    BOOL bRes;
    HANDLE iptHandle;
    IPT_INPUT_BUFFER inputBuffer;

    //
    // The trace comes as part of an output buffer, so that part is required
    //
    bRes = FALSE;
    if (dwTraceSize < UFIELD_OFFSET(IPT_OUTPUT_BUFFER, GetTrace.TraceSize))
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return bRes;
    }

    //
    // Open the IPT Device
    //
    bRes = OpenIptDevice(&iptHandle);
    if (bRes != FALSE)
    {
        //
        // Send a request to get the trace for the process
        //
        InitializeIptBuffer(&inputBuffer, IptGetProcessTrace);
        inputBuffer.GetProcessIptTrace.TraceVersion = IPT_TRACE_VERSION_CURRENT;
        inputBuffer.GetProcessIptTrace.ProcessHandle = hProcess;
        bRes = DeviceIoControl(iptHandle,
                               IOCTL_IPT_READ_TRACE,
                               &inputBuffer,
                               sizeof(inputBuffer),
                               pTrace,
                               dwTraceSize,
                               NULL,
                               NULL);
        CloseHandle(iptHandle);
    }
    return bRes;
}

BOOL
StartProcessIptTrace (
    _In_ HANDLE hProcess,
    _In_ IPT_OPTIONS ullOptions
    )
{
    BOOL bRes;
    HANDLE iptHandle;
    IPT_INPUT_BUFFER inputBuffer;
    IPT_OUTPUT_BUFFER outputBuffer;

    //
    // Open the IPT Device
    //
    bRes = OpenIptDevice(&iptHandle);
    if (bRes != FALSE)
    {
        //
        // Send a request to start tracing for this process
        //
        InitializeIptBuffer(&inputBuffer, IptStartProcessTrace);
        inputBuffer.StartProcessIptTrace.Options = ullOptions;
        inputBuffer.StartProcessIptTrace.ProcessHandle = hProcess;
        bRes = DeviceIoControl(iptHandle,
                               IOCTL_IPT_REQUEST,
                               &inputBuffer,
                               sizeof(inputBuffer),
                               &outputBuffer,
                               sizeof(outputBuffer),
                               NULL,
                               NULL);
        CloseHandle(iptHandle);
    }
    return bRes;
}

BOOL
StopProcessIptTrace (
    _In_ HANDLE hProcess
    )
{
    BOOL bRes;
    HANDLE iptHandle;
    IPT_INPUT_BUFFER inputBuffer;
    IPT_OUTPUT_BUFFER outputBuffer;

    //
    // Open the IPT Device
    //
    bRes = OpenIptDevice(&iptHandle);
    if (bRes != FALSE)
    {
        //
        // Send a request to start tracing for this process
        //
        InitializeIptBuffer(&inputBuffer, IptStopProcessTrace);
        inputBuffer.StopProcessIptTrace.ProcessHandle = hProcess;
        bRes = DeviceIoControl(iptHandle,
                               IOCTL_IPT_REQUEST,
                               &inputBuffer,
                               sizeof(inputBuffer),
                               &outputBuffer,
                               sizeof(outputBuffer),
                               NULL,
                               NULL);
        CloseHandle(iptHandle);
    }
    return bRes;
}

BOOL
StartCoreIptTracing (
    _In_ IPT_OPTIONS ullOptions,
    _In_ DWORD dwNumberOfTries,
    _In_ DWORD dwTraceDurationInSeconds
    )
{
    BOOL bRes;
    HANDLE iptHandle;
    IPT_INPUT_BUFFER inputBuffer;
    IPT_OUTPUT_BUFFER outputBuffer;

    //
    // Open the IPT Device
    //
    bRes = OpenIptDevice(&iptHandle);
    if (bRes != FALSE)
    {
        //
        // Send a request to start tracing for this process
        //
        InitializeIptBuffer(&inputBuffer, IptStartCoreTracing);
        inputBuffer.StartCoreIptTracing.Options = ullOptions;
        inputBuffer.StartCoreIptTracing.NumberOfTries = dwNumberOfTries;
        inputBuffer.StartCoreIptTracing.TraceDurationInSeconds = dwTraceDurationInSeconds;
        bRes = DeviceIoControl(iptHandle,
                               IOCTL_IPT_REQUEST,
                               &inputBuffer,
                               sizeof(inputBuffer),
                               &outputBuffer,
                               sizeof(outputBuffer),
                               NULL,
                               NULL);
        CloseHandle(iptHandle);
    }
    return bRes;
}

NTSTATUS
RegisterExtendedImageForIptTracing (
    _In_ PWCHAR pwszImagePath,
    _In_opt_ PWCHAR pwszFilteredPath,
    _In_ IPT_OPTIONS ullOptions,
    _In_ DWORD dwNumberOfTries,
    _In_ DWORD dwTraceDurationInSeconds
    )
{
    BOOL bRes;
    USHORT pathLength, filterLength;
    ULONG inputLength;
    HANDLE iptHandle;
    PIPT_INPUT_BUFFER inputBuffer;
    IPT_OUTPUT_BUFFER outputBuffer;

    //
    // Open the IPT Device
    //
    bRes = OpenIptDevice(&iptHandle);
    if (bRes != FALSE)
    {
        //
        // Compute the size of the image path, and input buffer containing it
        //
        pathLength = (USHORT)(wcslen(pwszImagePath) + 1) * sizeof(WCHAR);
        inputLength = pathLength + sizeof(IPT_INPUT_BUFFER);

        //
        // Add the IFEO filter path size if it was passed in
        //
        if (pwszFilteredPath != NULL)
        {
            filterLength = (USHORT)(wcslen(pwszFilteredPath) + 1) * sizeof(WCHAR);
            inputLength += filterLength;
        }

        //
        // Allocate the input buffer. Mimic Windows here by not using the heap.
        //
        inputBuffer = VirtualAlloc(NULL,
                                   inputLength,
                                   MEM_COMMIT,
                                   PAGE_READWRITE);
        if (inputBuffer != NULL)
        {
            //
            // Initialize a request for registering the given process
            //
            InitializeIptBuffer(inputBuffer, IptRegisterExtendedImageForTracing);
            inputBuffer->RegisterExtendedImageForIptTracing.Options = ullOptions;
            inputBuffer->RegisterExtendedImageForIptTracing.NumberOfTries = dwNumberOfTries;
            inputBuffer->RegisterExtendedImageForIptTracing.TraceDurationInSeconds = dwTraceDurationInSeconds;

            //
            // Copy the image path
            //
            inputBuffer->RegisterExtendedImageForIptTracing.ImagePathLength = pathLength;
            CopyMemory(inputBuffer->RegisterExtendedImageForIptTracing.ImageName,
                       pwszImagePath,
                       pathLength);

            //
            // Copy the filter path if it was present
            //
            if (pwszFilteredPath != NULL)
            {
                inputBuffer->RegisterExtendedImageForIptTracing.FilteredPathLength = filterLength;
                CopyMemory((PVOID)((ULONG_PTR)inputBuffer->RegisterExtendedImageForIptTracing.ImageName + pathLength),
                           pwszFilteredPath,
                           filterLength);
            }
            else
            {
                inputBuffer->RegisterExtendedImageForIptTracing.FilteredPathLength = 0;
            }

            //
            // Send the request
            //
            bRes = DeviceIoControl(iptHandle,
                                   IOCTL_IPT_REQUEST,
                                   &inputBuffer,
                                   sizeof(inputBuffer),
                                   &outputBuffer,
                                   sizeof(outputBuffer),
                                   NULL,
                                   NULL);

            //
            // Free the input buffer
            //
            VirtualFree(inputBuffer, 0, MEM_RELEASE);
        }
        else
        {
            //
            // Set failure since we're out of memory
            //
            bRes = FALSE;
        }
        CloseHandle(iptHandle);
    }
    return bRes;
}
