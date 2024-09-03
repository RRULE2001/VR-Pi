#include <ControllerDriver.h>

#include <winsock2.h>
#pragma comment(lib, "Ws2_32.lib")

EVRInitError ControllerDriver::Activate(uint32_t unObjectId)
{
    driverId = unObjectId; //unique ID for your driver

    PropertyContainerHandle_t props = VRProperties()->TrackedDeviceToPropertyContainer(driverId); //this gets a container object where you store all the information about your driver

    VRProperties()->SetStringProperty(props, Prop_InputProfilePath_String, "{example}/input/controller_profile.json"); //tell OpenVR where to get your driver's Input Profile
    VRProperties()->SetInt32Property(props, Prop_ControllerRoleHint_Int32, ETrackedControllerRole::TrackedControllerRole_Treadmill); //tells OpenVR what kind of device this is
    VRDriverInput()->CreateScalarComponent(props, "/input/joystick/y", &joystickYHandle, EVRScalarType::VRScalarType_Absolute,
        EVRScalarUnits::VRScalarUnits_NormalizedTwoSided); //sets up handler you'll use to send joystick commands to OpenVR with, in the Y direction (forward/backward)
    VRDriverInput()->CreateScalarComponent(props, "/input/trackpad/y", &trackpadYHandle, EVRScalarType::VRScalarType_Absolute,
        EVRScalarUnits::VRScalarUnits_NormalizedTwoSided); //sets up handler you'll use to send trackpad commands to OpenVR with, in the Y direction
    VRDriverInput()->CreateScalarComponent(props, "/input/joystick/x", &joystickXHandle, EVRScalarType::VRScalarType_Absolute,
        EVRScalarUnits::VRScalarUnits_NormalizedTwoSided); //Why VRScalarType_Absolute? Take a look at the comments on EVRScalarType.
    VRDriverInput()->CreateScalarComponent(props, "/input/trackpad/x", &trackpadXHandle, EVRScalarType::VRScalarType_Absolute,
        EVRScalarUnits::VRScalarUnits_NormalizedTwoSided); //Why VRScalarUnits_NormalizedTwoSided? Take a look at the comments on EVRScalarUnits.

    //The following properites are ones I tried out because I saw them in other samples, but I found they were not needed to get the sample working.
    //There are many samples, take a look at the openvr_header.h file. You can try them out.

    //VRProperties()->SetUint64Property(props, Prop_CurrentUniverseId_Uint64, 2);
    //VRProperties()->SetBoolProperty(props, Prop_HasControllerComponent_Bool, true);
    //VRProperties()->SetBoolProperty(props, Prop_NeverTracked_Bool, true);
    //VRProperties()->SetInt32Property(props, Prop_Axis0Type_Int32, k_eControllerAxis_TrackPad);
    //VRProperties()->SetInt32Property(props, Prop_Axis2Type_Int32, k_eControllerAxis_Joystick);
    //VRProperties()->SetStringProperty(props, Prop_SerialNumber_String, "example_controler_serial");
    //VRProperties()->SetStringProperty(props, Prop_RenderModelName_String, "vr_controller_vive_1_5");
    //uint64_t availableButtons = ButtonMaskFromId(k_EButton_SteamVR_Touchpad) |
    //	ButtonMaskFromId(k_EButton_IndexController_JoyStick);
    //VRProperties()->SetUint64Property(props, Prop_SupportedButtons_Uint64, availableButtons);

    return VRInitError_None;
}

DriverPose_t ControllerDriver::GetPose()
{
    DriverPose_t pose = { 0 }; //This example doesn't use Pose, so this method is just returning a default Pose.
    pose.poseIsValid = false;
    pose.result = TrackingResult_Calibrating_OutOfRange;
    pose.deviceIsConnected = true;

    HmdQuaternion_t quat;
    quat.w = 1;
    quat.x = 0;
    quat.y = 0;
    quat.z = 0;

    pose.qWorldFromDriverRotation = quat;
    pose.qDriverFromHeadRotation = quat;

    return pose;
}

void ControllerDriver::RunFrame()
{
    //Since we used VRScalarUnits_NormalizedTwoSided as the unit, the range is -1 to 1.
    VRDriverLog()->Log("Server: Run Handler\n"); //this is how you log out Steam's log file.
    float data[2];
    server_Handle(data);

    VRDriverInput()->UpdateScalarComponent(joystickYHandle, data[1], 0); //move forward
    VRDriverInput()->UpdateScalarComponent(trackpadYHandle, data[1], 0); //move foward
    VRDriverInput()->UpdateScalarComponent(joystickXHandle, data[0], 0); //change the value to move sideways
    VRDriverInput()->UpdateScalarComponent(trackpadXHandle, data[0], 0); //change the value to move sideways
}

void ControllerDriver::Deactivate()
{
    driverId = k_unTrackedDeviceIndexInvalid;
}

void* ControllerDriver::GetComponent(const char* pchComponentNameAndVersion)
{
    //I found that if this method just returns null always, it works fine. But I'm leaving the if statement in since it doesn't hurt.
    //Check out the IVRDriverInput_Version declaration in openvr_driver.h. You can search that file for other _Version declarations 
    //to see other components that are available. You could also put a log in this class and output the value passed into this 
    //method to see what OpenVR is looking for.
    if (strcmp(IVRDriverInput_Version, pchComponentNameAndVersion) == 0)
    {
        return this;
    }
    return NULL;
}

void ControllerDriver::EnterStandby() {}

void ControllerDriver::DebugRequest(const char* pchRequest, char* pchResponseBuffer, uint32_t unResponseBufferSize)
{
    if (unResponseBufferSize >= 1)
    {
        pchResponseBuffer[0] = 0;
    }
}

void ControllerDriver::server_Handle(float* joystick) {
    float result[2];
    result[0] = 0.95f;
    result[1] = 0.95f;
    joystick[0] = 0.95f;
    joystick[1] = 0.95f;
    WSADATA wsaData;
    SOCKET ReceivingSocket;
    SOCKADDR_IN ReceiverAddr;
    int Port = 50000;
    char ReceiveBuf[1024];
    int BufLength = 1024;
    SOCKADDR_IN SenderAddr;
    int SenderAddrSize = sizeof(SenderAddr);
    int ByteReceived = 5;

    // Initialize Winsock version 2.2

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {

        //printf("Server: WSAStartup failed with error: %ld\n", WSAGetLastError());
        VRDriverLog()->Log("Server: WSAStartup failed with error: %ld\n");// + WSAGetLastError());

        return;
    }
    else {
        //printf("Server: The Winsock DLL status is: %s.\n", wsaData.szSystemStatus);
        VRDriverLog()->Log("Server: The Winsock DLL status is: %s.\n");// +wsaData.szSystemStatus);
    }

    // Create a new socket to receive datagrams on.

    ReceivingSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (ReceivingSocket == INVALID_SOCKET) {

        //Print error message
        //printf("Server: Error at socket(): %ld\n", WSAGetLastError());
        VRDriverLog()->Log("Server: Error at socket(): %ld\n");// + WSAGetLastError());

        // Clean up
        WSACleanup();

        // Exit with error
        return;
    }
    else {
        //printf("Server: socket() is OK!\n");
        VRDriverLog()->Log("Server: socket() is OK!\n");
    }

    /*Set up a SOCKADDR_IN structure that will tell bind that
    we want to receive datagrams from all interfaces using port 5150.*/


    // The IPv4 family
    ReceiverAddr.sin_family = AF_INET;

    // Port no. (8888)
    ReceiverAddr.sin_port = htons(Port);

    // From all interface (0.0.0.0)
    ReceiverAddr.sin_addr.s_addr = htonl(INADDR_ANY);



    // Associate the address information with the socket using bind.

    // At this point you can receive datagrams on your bound socket.

    if (bind(ReceivingSocket, (SOCKADDR*)&ReceiverAddr, sizeof(ReceiverAddr)) == SOCKET_ERROR) {

        // Print error message
        //printf("Server: Error! bind() failed!\n");
        VRDriverLog()->Log("Server: Error! bind() failed!\n");

        // Close the socket
        closesocket(ReceivingSocket);

        // Do the clean up
        WSACleanup();

        // and exit with error
        return;
    }
    else {
        //printf("Server: bind() is OK!\n");
        VRDriverLog()->Log("Server: bind() is OK!\n");
    }


    // Print some info on the receiver(Server) side...
    //getsockname(ReceivingSocket, (SOCKADDR *)&ReceiverAddr, (int *)sizeof(ReceiverAddr));


    //printf("Server: Receiving IP(s) used: %s\n", inet_ntoa(ReceiverAddr.sin_addr));

    //printf("Server: Receiving port used: %d\n", htons(ReceiverAddr.sin_port));

    //printf("Server: I\'m ready to receive data packages. Waiting...\n\n");


    // At this point you can receive datagrams on your bound socket.
    int read = 0;
    while (read == 0) { // Server is receiving data until you will close it.(You can replace while(1) with a condition to stop receiving.)
        VRDriverLog()->Log("Server: Trying to get a reading\n");
        memcpy(ReceiveBuf, "", sizeof ReceiveBuf);
        ByteReceived = recvfrom(ReceivingSocket, ReceiveBuf, BufLength, 0, (SOCKADDR*)&SenderAddr, &SenderAddrSize);
        VRDriverLog()->Log("Server: ByteReceived \n" + ByteReceived);
        if (ByteReceived > 0) { //If there are data
            //Print information for received data
            //printf("Server: Total Bytes received: %d\n", ByteReceived);
            //printf("Server: The data is: %s\n", ReceiveBuf);
            //printf("\n");
            VRDriverLog()->Log("Server: Total Bytes received: %d\n");// + ByteReceived);
            VRDriverLog()->Log("Server: The data is: %s\n");// + (char)ReceiveBuf);
            VRDriverLog()->Log("\n");

            char msg[1024];
            memcpy(msg, ReceiveBuf, ByteReceived);

            char* pIndex;
            int xIndex, xStart, yIndex, yStart;
            char xString[8] = "";
            char yString[8] = "";

            pIndex = strchr(msg, 'X');
            xIndex = (int)(pIndex - msg);
            xStart = xIndex + 2;
            pIndex = strchr(msg, 'Y');
            yIndex = (int)(pIndex - msg);
            yStart = yIndex + 2;

            memcpy(xString, msg + xStart, yIndex - xIndex - 3);
            memcpy(yString, msg + yStart, ByteReceived - yIndex - 2);
            float x = atof(xString);
            float y = atof(yString);
            read++;
            joystick[0] = x;
            joystick[1] = y;

            //printf("X index:%d\n", xIndex);
            //printf("X start:%d\n", xStart);
            //printf("Y index: %d\n", yIndex);
            //printf("Y start:%d\n", yStart);
            //printf("X length:%d\n", yIndex-xIndex-3);
            //printf("Y length:%d\n", ByteReceived-yIndex-2);
            //printf("X string:%s\n", xString);
            //printf("Y string:%s\n", yString);
            //printf("X float:%.3f\n", x);
            //printf("Y float:%.3f\n", y);

        }
        else if (ByteReceived <= 0) { //If the buffer is empty
            //Print error message
            //printf("Server: Connection closed with error code: %ld\n", WSAGetLastError());
            VRDriverLog()->Log("Server: Connection closed with error code: %ld\n");// + WSAGetLastError());
        }
        else { //If error
            //Print error message
            //printf("Server: recvfrom() failed with error code: %d\n", WSAGetLastError());
            VRDriverLog()->Log("Server: recvfrom() failed with error code: %d\n");// + WSAGetLastError());
        }
    }


    // Print some info on the sender(Client) side...
    //getpeername(ReceivingSocket, (SOCKADDR*)&SenderAddr, &SenderAddrSize);
    //printf("Server: Sending IP used: %s\n", inet_ntoa(SenderAddr.sin_addr));
    VRDriverLog()->Log("Server: Sending IP used: %s\n");// +inet_ntoa(SenderAddr.sin_addr));
    //printf("Server: Sending port used: %d\n", htons(SenderAddr.sin_port));
    VRDriverLog()->Log("Server: Sending port used: %d\n");// + htons(SenderAddr.sin_port));


    // When your application is finished receiving datagrams close the socket.
    //printf("Server: Finished receiving. Closing the listening socket...\n");
    VRDriverLog()->Log("Server: Finished receiving. Closing the listening socket...\n");
    if (closesocket(ReceivingSocket) != 0) {
        //printf("Server: closesocket() failed! Error code: %ld\n", WSAGetLastError());
        VRDriverLog()->Log("Server: closesocket() failed! Error code: %ld\n");// + WSAGetLastError());
    }
    else {
        //printf("Server: closesocket() is OK\n");
        VRDriverLog()->Log("Server: closesocket() is OK\n");
    }


    // When your application is finished call WSACleanup.
    //printf("Server: Cleaning up...\n");
    VRDriverLog()->Log("Server: Cleaning up...\n");

    if (WSACleanup() != 0) {
        //printf("Server: WSACleanup() failed! Error code: %ld\n", WSAGetLastError());
        VRDriverLog()->Log("Server: WSACleanup() failed! Error code: %ld\n");// + WSAGetLastError());
    }
    else {
        //printf("Server: WSACleanup() is OK\n");
        VRDriverLog()->Log("Server: WSACleanup() is OK\n");
    }

    // Back to the system
    return;
}