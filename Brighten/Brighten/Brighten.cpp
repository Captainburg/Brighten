#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <CL/cl.h>
#define DATA_SIZE (1024)

HBITMAP hBitmap;
BITMAP Bitmap;
BITMAPFILEHEADER bmfh;
BITMAPINFO     * pbmi;
BYTE* pBits;

LRESULT CALLBACK HelloWndProc(HWND, UINT, WPARAM, LPARAM);
BYTE* openCLbrighten(BYTE* buffer, int bmWidth, int bmHeight, LARGE_INTEGER* start, LARGE_INTEGER* end);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	PSTR szCMLine, int iCmdShow) {
	static TCHAR szAppName[] = TEXT("HelloApplication");//name of app
	HWND	hwnd;//holds handle to the main window
	MSG		msg;//holds any message retrieved from the msg queue
	WNDCLASS wndclass;//wnd class for registration

					  //defn wndclass attributes for this application
	wndclass.style = CS_HREDRAW | CS_VREDRAW;//redraw on refresh both directions
	wndclass.lpfnWndProc = HelloWndProc;//wnd proc to handle windows msgs/commands
	wndclass.cbClsExtra = 0;//class space for expansion/info carrying
	wndclass.cbWndExtra = 0;//wnd space for info carrying
	wndclass.hInstance = hInstance;//application instance handle
	wndclass.hIcon = LoadIcon(NULL, IDI_APPLICATION);//set icon for window
	wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);//set cursor for window
	wndclass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);//set background
	wndclass.lpszMenuName = NULL;//set menu
	wndclass.lpszClassName = szAppName;//set application name

									   //register wndclass to O/S so approp. wnd msg are sent to application
	if (!RegisterClass(&wndclass)) {
		MessageBox(NULL, TEXT("This program requires Windows 95/98/NT"),
			szAppName, MB_ICONERROR);//if unable to be registered
		return 0;
	}
	//create the main window and get it's handle for future reference
	hwnd = CreateWindow(szAppName,		//window class name
		TEXT("Hello World for Windows"), // window caption
		WS_OVERLAPPEDWINDOW,	//window style
		CW_USEDEFAULT,		//initial x position
		CW_USEDEFAULT,		//initial y position
		CW_USEDEFAULT,		//initial x size
		CW_USEDEFAULT,		//initial y size
		NULL,				//parent window handle
		NULL,				//window menu handle
		hInstance,			//program instance handle
		NULL);				//creation parameters
	ShowWindow(hwnd, iCmdShow);//set window to be shown
	UpdateWindow(hwnd);//force an update so window is drawn

					   //messgae loop
	while (GetMessage(&msg, NULL, 0, 0)) {//get message from queue
		TranslateMessage(&msg);//for keystroke translation
		DispatchMessage(&msg);//pass msg back to windows for processing
							  //note that this is to put windows o/s in control, rather than this app
	}

	return msg.wParam;
}

BYTE* rgbToRGBA(BYTE* rgb, int bmWidth, int bmHeight) {
	BYTE* RGBA = (BYTE*)malloc(bmWidth * bmHeight * 4);
	for (int i = 0; i < bmWidth * bmHeight; i++) {
		RGBA[i * 4] = rgb[i * 3];
		RGBA[i * 4 + 1] = rgb[i * 3 + 1];
		RGBA[i * 4 + 2] = rgb[i * 3 + 2];
		RGBA[i * 4 + 3] = 255;
	}
	return RGBA;
}

BYTE* rgbaToRGB(BYTE* rgba, int bmWidth, int bmHeight) {
	BYTE* RGB = (BYTE*)malloc(bmWidth * bmHeight * 3);
	for (int i = 0; i < bmWidth * bmHeight; i++) {
		RGB[i * 3] = rgba[i * 4];
		RGB[i * 3 + 1] = rgba[i * 4 + 1];
		RGB[i * 3 + 2] = rgba[i * 4 + 2];
	}
	return RGB;
}

void nonAsMbrighten(BITMAP* bitmap, INT brighten, BYTE* temppBits) {
	INT width = bitmap->bmWidth;
	INT height = bitmap->bmHeight;
	INT bitsperPixel = bitmap->bmBitsPixel;
	BYTE value = 0;//temp location of brighten value

	for (int i = 0; i < height * 3; i++) {
		for (int j = 0; j < width; j++) {
			value = temppBits[i*width + j];
			value = min(255, value + brighten);//deal with rollover via saturation
			temppBits[i*width + j] = value;
		}
	}
}
/**
mmx registers: mm0-mm7 - general purpose
pxor - packed xor between two registers
movd - moves a double word (32 bits) between memory and reg or reg to reg
por - packed "or" between two registers
psllq - packed shift left quad (4 DWORDS)
movq - move quad (4 DWORDS or 64 bits) between memory and reg or reg to reg
paddusb - adds unsigned bytes (8 bytes) with saturation between two reg

*/
void mmx_brighten(BITMAP* bitmap, byte brighten, BYTE* buffer) {
	INT width = bitmap->bmWidth;
	INT height = bitmap->bmHeight;
	INT bitsPerPixel = bitmap->bmBitsPixel;
	INT bytesPerPixel = bitsPerPixel / 8;
	byte brightenA[8] = { brighten, brighten, brighten, brighten, brighten, brighten, brighten, brighten };

	//EAX, EBX, ECX, EDX are general purpose registers
	//ESI, EDI, EBP are also available as general purpose registers
	//AX, BX, CX, DX are the lower 16-bit of the above registers (think E as extended)
	//AH, AL, BH, BL, CH, CL, DH, DL are 8-bit high/low registers of the above (AX, BX, etc)
	//Typical use:
	//EAX accumulator for operands and results
	//EBX base pointer to data in the data segment
	//ECX counter for loops
	//EDX data pointer and I/O pointer
	//EBP frame pointer for stack frames
	//ESP stack pointer hardcoded into PUSH/POP operations
	//ESI source index for array operations
	//EDI destination index for array operations [e.g. copying arrays]
	//EIP instruction pointer
	//EFLAGS results flag hardcoded into conditional operations

	__asm
	{
		// save all registers you will be using onto stack
		push edi
		push esi

		// calculate the number of bytes in image (pixels*bitsperpixel)
		mov esi, width
		imul esi, height
		imul esi, bytesPerPixel

		//setup counter register
		sub esi, 8

		// store the address of the buffer into a register (e.g. ebx)
		mov edi, buffer

		//Setup Brighten Value in mm0
		movq mm0, brightenA

		//create a loop
		myLoop :

		//load a pixel into a register A R G B
		movq mm1, [edi + esi]

		//add brighten value to each pixel
		paddusb mm1, mm0

		//store back into buffer
		movq [edi + esi], mm1

		//decrement loop counter by 3
		sub esi, 8

		//loop back up
		cmp esi, 0
		jge myLoop

		emms

		//restore registers to original values before leaving
		pop esi
		pop edi
	}

	//clear mm2 reg

	//store brighten value

	//brighten value needs to be in each byte of an mmx reg
	//loop and shift and load brighten value and "or"
	//until each byte in an mmx reg holds brighten value
	//use mm0 to hold value
	//note: can't use mm2 as work (calc) can only be done
	//using mmx reg. Only loading in a value can be done using
	//memory and mmx reg

	//clear ecx reg to use as counter

	//start a loop
	//end loop if number of loops is greater than bytes
	//in image/8

	//load 8 bytes into mm1 

	//add brighten value with saturation

	//copy brighten value back to buffer

	//move the buffer pointer position by 8
	//since we are grabbing 8 bytes at once

	//inc our counter (ecx)

	//loop back to repeat

	//return reg values from stack

	//end mmx (emms)

}
void assembly_brighten(BITMAP* bitmap, INT brighten, BYTE* buffer) {
	INT width = bitmap->bmWidth;
	INT height = bitmap->bmHeight;
	INT bitsPerPixel = bitmap->bmBitsPixel;
	INT bytesPerPixel = bitsPerPixel / 8;
	//REGISTERS

	//EAX, EBX, ECX, EDX are general purpose registers
	//ESI, EDI, EBP are also available as general purpose registers
	//AX, BX, CX, DX are the lower 16-bit of the above registers (think E as extended)
	//AH, AL, BH, BL, CH, CL, DH, DL are 8-bit high/low registers of the above (AX, BX, etc)
	//Typical use:
	//EAX accumulator for operands and results
	//EBX base pointer to data in the data segment
	//ECX counter for loops
	//EDX data pointer and I/O pointer
	//EBP frame pointer for stack frames
	//ESP stack pointer hardcoded into PUSH/POP operations
	//ESI source index for array operations
	//EDI destination index for array operations [e.g. copying arrays]
	//EIP instruction pointer
	//EFLAGS results flag hardcoded into conditional operations

	//MOV <source>, <destination>: mov reg, reg; mov reg, immediate; mov reg, memory; mov mem, reg; mov mem, imm
	//INC and DEC on registers or memory
	//ADD destination, source
	//SUB destination, source
	//CMP destination, source : sets the appropriate flag after performing (destination) - (source)
	//JMP label - jumps unconditionally ie. always to location marked by "label"
	//JE - jump if equal, JG/JL - jump if greater/less, JGE/JLE if greater or equal/less or equal, JNE - not equal, JZ - zero flag set
	//LOOP target: uses ECX to decrement and jump while ECX>0
	//logical instructions: AND, OR, XOR, NOT - performs bitwise logical operations. Note TEST is non-destructive AND instruction
	//SHL destination, count : shift left, SHR destination, count :shift right - carry flag (CF) and zero (Z) bits used, CL register often used if shift known
	//ROL - rotate left, ROR rotate right, RCL (rotate thru carry left), RCR (rotate thru carry right)
	//EQU - used to elimate hardcoding to create constants
	//MUL destination, source : multiplication
	//PUSH <source> - pushes source onto the stack
	//POP <destination> - pops off the stack into destination
	__asm
	{
		// save all registers you will be using onto stack
		push eax
		push ebx
		push ecx
		push edx
		push edi
		push esi

		// calculate the number of bytes in image (pixels*bitsperpixel)
		mov eax, width
		imul eax, height
		imul eax, bytesPerPixel

		//setup counter register
		mov esi, eax
		sub esi, 4

		// store the address of the buffer into a register (e.g. ebx)
		mov edi, buffer

		//create a loop
		myLoop:

		//load a pixel into a register A R G B
		mov eax, [edi + esi]
		mov ebx, eax
		mov ecx, eax
		mov edx, eax

		//shift bits down for each channel
		//R
		shl eax, 8
		shr eax, 24
		//G
		shl ebx, 16
		shr ebx, 24
		//B
		shl ecx, 24
		shr ecx, 24
		//A
		shr edx, 24
		shl edx, 24

		//add brighten value to each pixel
		add eax, brighten
		add ebx, brighten
		add ecx, brighten

		//check each pixel to see if > 255
		cmp eax, 255
		jle passRed
		mov eax, 255
		passRed:

		cmp ebx, 255
		jle passGreen
		mov ebx, 255
		passGreen:

		cmp ecx, 255
		jle passBlue
		mov ecx, 255
		passBlue:

		//put pixel back together again
		//R
		shl eax, 16
		//G
		shl ebx, 8
		//B
		//shl ecx, 0

		//add each channel
		add eax, ebx
		add eax, ecx
		add eax, edx

		//store back into buffer
		mov [edi + esi], eax

		//decrement loop counter by 3
		sub esi, bytesPerPixel

		//loop back up
		cmp esi, 0
		jge myLoop

		//restore registers to original values before leaving
		pop esi
		pop edi
		pop edx
		pop ecx
		pop ebx
		pop eax

		//function
	}
}

////////////////////////////////////////////////////////////////////////////////

// Simple compute kernel which computes the brightenPixel of an input array 
//
const char *KernelSource = "\n" \
"__kernel void brightenPixel(                                         \n" \
"   __read_only image2d_t input,                                              \n" \
"   __write_only image2d_t output)                                           \n" \
"{                                                                      \n" \
"   const sampler_t samplerA = CLK_NORMALIZED_COORDS_FALSE                \n"\
"   | CLK_ADDRESS_CLAMP_TO_EDGE                                           \n"\
"   | CLK_FILTER_NEAREST;                                                \n" \
"                                                                      \n" \
"   int2 myCoor;                                                       \n" \
"   myCoor.x = get_global_id(0);                                                                   \n" \
"   myCoor.y = get_global_id(1);                                                                   \n" \
"                                                                      \n" \
"   uint4 pix = read_imageui(input, samplerA, myCoor);  \n" \
"   pix.x = pix.x * 1.001;                                                                   \n" \
"   pix.y = pix.y * 1.001;                                                                   \n" \
"   pix.z = pix.z * 1.001;                                                                   \n" \
"   write_imageui(output, myCoor, pix);                 \n" \
"}                                                                      \n" \
"\n";

////////////////////////////////////////////////////////////////////////////////

/**
Purpose: To handle windows messages for specific cases including when
the window is first created, refreshing (painting), and closing
the window.

Returns: Long - any error message (see Win32 API for details of possible error messages)
Notes:	 CALLBACK is defined as __stdcall which defines a calling
convention for assembly (stack parameter passing)
**/
LRESULT CALLBACK HelloWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	HDC		hdc;
	PAINTSTRUCT ps;
	RECT	rect;
	BITMAP* bitmap;
	HDC hdcMem;
	BOOL bSuccess;
	DWORD dwBytesRead, dwInfoSize;
	HANDLE hFile;
	int error = 0;
	LARGE_INTEGER endTime;
	LARGE_INTEGER initialTime;
	char timeStr[512];
	BYTE *temppBits;
	BYTE* resultBits;
	BYTE* rgba;
	BYTE* temp2;
	static int selector = 0;

	switch (message) {
	case WM_CREATE:
		hFile = CreateFile(TEXT("splash.bmp"), GENERIC_READ, FILE_SHARE_READ,
			NULL, OPEN_EXISTING, 0, NULL);

		if (hFile == INVALID_HANDLE_VALUE) {
			error = GetLastError();
			return 0;
		}

		error = sizeof(BITMAPFILEHEADER);

		bSuccess = ReadFile(hFile, &bmfh, sizeof(BITMAPFILEHEADER),
			&dwBytesRead, NULL);

		if (!bSuccess || (dwBytesRead != sizeof(BITMAPFILEHEADER))
			|| (bmfh.bfType != *(WORD *) "BM"))
		{
			//			CloseHandle(hFile);
			return NULL;
		}
		dwInfoSize = bmfh.bfOffBits - sizeof(BITMAPFILEHEADER);

		pbmi = (BITMAPINFO*)malloc(dwInfoSize);

		bSuccess = ReadFile(hFile, pbmi, dwInfoSize, &dwBytesRead, NULL);

		if (!bSuccess || (dwBytesRead != dwInfoSize))
		{
			free(pbmi);
			CloseHandle(hFile);
			return NULL;
		}
		hBitmap = CreateDIBSection(NULL, pbmi, DIB_RGB_COLORS, (VOID**)&pBits, NULL, 0);
		ReadFile(hFile, pBits, bmfh.bfSize - bmfh.bfOffBits, &dwBytesRead, NULL);

		GetObject(hBitmap, sizeof(BITMAP), &Bitmap);
		return 0;

	case WM_LBUTTONDOWN:
		hdc = GetDC(hwnd);
		hdcMem = CreateCompatibleDC(hdc);
		GetObject(hBitmap, sizeof(BITMAP), &Bitmap);
		temppBits = (BYTE*)malloc(Bitmap.bmWidth*Bitmap.bmHeight * 3);
		memcpy(temppBits, pBits, Bitmap.bmWidth*Bitmap.bmHeight * 3);
		hBitmap = CreateDIBSection(NULL, pbmi, DIB_RGB_COLORS, (VOID**)&pBits, NULL, 0);

		//REGULAR BLOCK
		QueryPerformanceCounter(&initialTime);
		nonAsMbrighten(&Bitmap, 4, temppBits);
		QueryPerformanceCounter(&endTime);
		sprintf_s(timeStr, "REG: %d%d", endTime.HighPart - initialTime.HighPart, endTime.LowPart - initialTime.LowPart);

		//ASM BLOCK
		QueryPerformanceCounter(&initialTime);
		assembly_brighten(&Bitmap, 4, temppBits);
		QueryPerformanceCounter(&endTime);
		sprintf_s(timeStr, "%s, ASM: %d%d", timeStr, endTime.HighPart - initialTime.HighPart, endTime.LowPart - initialTime.LowPart);

		//MMX BLOCK
		QueryPerformanceCounter(&initialTime);
		mmx_brighten(&Bitmap, 4, temppBits);
		QueryPerformanceCounter(&endTime);
		sprintf_s(timeStr, "%s, MMX: %d%d", timeStr, endTime.HighPart - initialTime.HighPart, endTime.LowPart - initialTime.LowPart);

		//OPENCL BLOCK
		rgba = rgbToRGBA(temppBits, Bitmap.bmWidth, Bitmap.bmHeight);
		temp2 = openCLbrighten(rgba, Bitmap.bmWidth, Bitmap.bmHeight, &initialTime, &endTime);
		resultBits = rgbaToRGB(temp2, Bitmap.bmWidth, Bitmap.bmHeight);
		sprintf_s(timeStr, "%s, OPENCL: %d%d", timeStr, endTime.HighPart - initialTime.HighPart, endTime.LowPart - initialTime.LowPart);

		SetWindowText(hwnd, timeStr);
		memcpy(pBits, resultBits, Bitmap.bmWidth*Bitmap.bmHeight * 3);
		SelectObject(hdcMem, hBitmap);

		BitBlt(hdc, 0, 0, Bitmap.bmWidth, Bitmap.bmHeight,
			hdcMem, 0, 0, SRCCOPY);

		//Free Block
		free(temppBits);
		free(rgba);
		free(temp2);
		free(resultBits);
		return 0;

	case WM_RBUTTONDOWN:
		hdc = GetDC(hwnd);
		hdcMem = CreateCompatibleDC(hdc);
		GetObject(hBitmap, sizeof(BITMAP), &Bitmap);
		temppBits = (BYTE*)malloc(Bitmap.bmWidth*Bitmap.bmHeight * 3);
		memcpy(temppBits, pBits, Bitmap.bmWidth*Bitmap.bmHeight * 3);
		hBitmap = CreateDIBSection(NULL, pbmi, DIB_RGB_COLORS, (VOID**)&pBits, NULL, 0);

		switch (selector) {
		case 0:
			//REGULAR BLOCK
			QueryPerformanceCounter(&initialTime);
			nonAsMbrighten(&Bitmap, 30, temppBits);
			QueryPerformanceCounter(&endTime);
			sprintf_s(timeStr, "REG: %d%d", endTime.HighPart - initialTime.HighPart, endTime.LowPart - initialTime.LowPart);
			break;
		case 1:
			//ASM BLOCK
			QueryPerformanceCounter(&initialTime);
			assembly_brighten(&Bitmap, 30, temppBits);
			QueryPerformanceCounter(&endTime);
			sprintf_s(timeStr, "ASM: %d%d", endTime.HighPart - initialTime.HighPart, endTime.LowPart - initialTime.LowPart);
			break;
		case 2:
			//MMX BLOCK
			QueryPerformanceCounter(&initialTime);
			mmx_brighten(&Bitmap, 30, temppBits);
			QueryPerformanceCounter(&endTime);
			sprintf_s(timeStr, "MMX: %d%d", endTime.HighPart - initialTime.HighPart, endTime.LowPart - initialTime.LowPart);
			break;
		case 3:
			//OPENCL BLOCK
			rgba = rgbToRGBA(temppBits, Bitmap.bmWidth, Bitmap.bmHeight);
			temp2 = openCLbrighten(rgba, Bitmap.bmWidth, Bitmap.bmHeight, &initialTime, &endTime);
			temppBits = rgbaToRGB(temp2, Bitmap.bmWidth, Bitmap.bmHeight);
			sprintf_s(timeStr, "OPENCL: %d%d", endTime.HighPart - initialTime.HighPart, endTime.LowPart - initialTime.LowPart);
			break;
		}
		selector++;
		if (selector == 4) {
			selector = 0;
		}

		SetWindowText(hwnd, timeStr);
		memcpy(pBits, temppBits, Bitmap.bmWidth*Bitmap.bmHeight * 3);
		SelectObject(hdcMem, hBitmap);

		BitBlt(hdc, 0, 0, Bitmap.bmWidth, Bitmap.bmHeight,
			hdcMem, 0, 0, SRCCOPY);

		//Free Block
		return 0;

	case WM_PAINT://what to do when a paint msg occurs
		hdc = BeginPaint(hwnd, &ps);//get a handle to a device context for drawing
		GetClientRect(hwnd, &rect);//define drawing area for clipping
								   //GetObject(hBitmap, sizeof(BITMAP), &Bitmap);
		hdcMem = CreateCompatibleDC(hdc);
		SelectObject(hdcMem, hBitmap);

		BitBlt(hdc, 0, 0, Bitmap.bmWidth, Bitmap.bmHeight,
			hdcMem, 0, 0, SRCCOPY);


		EndPaint(hwnd, &ps);//release the device context
		return 0;

	case WM_DESTROY://how to handle a destroy (close window app) msg
		PostQuitMessage(0);
		return 0;
	}
	//return the message to windows for further processing
	return DefWindowProc(hwnd, message, wParam, lParam);
}

BYTE* openCLbrighten(BYTE* buffer, int bmWidth, int bmHeight, LARGE_INTEGER* start, LARGE_INTEGER* end) {
	int err;                            // error code returned from api calls

	size_t global[2];                      // global domain size for our calculation
	size_t local[2];                       // local domain size for our calculation

	BYTE* results = (BYTE*)malloc(bmWidth * bmHeight * 4);

	cl_device_id device_id;             // compute device id 
	cl_context context;                 // compute context
	cl_command_queue commands;          // compute command queue
	cl_program program;                 // compute program
	cl_kernel kernel;                   // compute kernel

	cl_mem input;                       // device memory used for the input array
	cl_mem output;                      // device memory used for the output array

	cl_uint num_of_platforms = 0;
	// get total number of available platforms:
	err = clGetPlatformIDs(0, 0, &num_of_platforms);
	cl_platform_id* platforms = new cl_platform_id[num_of_platforms];
	//    // get IDs for all platforms:
	err = clGetPlatformIDs(num_of_platforms, platforms, 0);
	// Connect to a compute device
	int gpu = 1;
	err = clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_GPU, 1, &device_id, 0);
	if (err == CL_INVALID_PLATFORM)
		printf("invalid platform");
	if (err == CL_INVALID_DEVICE_TYPE)
		printf("invalid device type");
	if (err == CL_INVALID_VALUE)
		printf("invalid value");
	if (err == CL_DEVICE_NOT_FOUND)
		printf("device not found");
	if (err == CL_OUT_OF_RESOURCES)
		printf("out of resources");
	if (err != CL_SUCCESS)
	{
		printf("Error: Failed to create a device group!\n");
		///return EXIT_FAILURE;
	}

	// Create a compute context 
	//
	context = clCreateContext(0, 1, &device_id, NULL, NULL, &err);
	if (!context)
	{
		printf("Error: Failed to create a compute context!\n");
		//return EXIT_FAILURE;
	}

	// Create a command commands
	//
	commands = clCreateCommandQueue(context, device_id, 0, &err);
	if (!commands)
	{
		printf("Error: Failed to create a command commands!\n");
		//return EXIT_FAILURE;
	}

	// Create the compute program from the source buffer
	//
	program = clCreateProgramWithSource(context, 1, (const char **)& KernelSource, NULL, &err);
	if (!program)
	{
		printf("Error: Failed to create compute program!\n");
		//return EXIT_FAILURE;
	}

	// Build the program executable
	//
	err = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
	if (err != CL_SUCCESS)
	{
		size_t len;
		char buffer[2048];

		printf("Error: Failed to build program executable!\n");
		clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);
		printf("%s\n", buffer);
		exit(1);
	}

	// Create the compute kernel in the program we wish to run
	kernel = clCreateKernel(program, "brightenPixel", &err);
	if (!kernel || err != CL_SUCCESS)
	{
		printf("Error: Failed to create compute kernel!\n");
		exit(1);
	}

	// Create the input and output arrays in device memory for our calculation
	cl_image_format* myFormatI = (cl_image_format*)malloc(sizeof(cl_image_format));
	myFormatI->image_channel_order = CL_RGBA;
	myFormatI->image_channel_data_type = CL_UNORM_INT8;

	cl_image_desc* myDescI = (cl_image_desc*)calloc(1, sizeof(cl_image_desc));
	myDescI->image_type = CL_MEM_OBJECT_IMAGE2D;
	myDescI->image_width = bmWidth;
	myDescI->image_height = bmHeight;

	cl_image_format* myFormatO = (cl_image_format*)malloc(sizeof(cl_image_format));
	myFormatO->image_channel_order = CL_RGBA;
	myFormatO->image_channel_data_type = CL_UNORM_INT8;

	cl_image_desc* myDescO = (cl_image_desc*)calloc(1, sizeof(cl_image_desc));
	myDescO->image_type = CL_MEM_OBJECT_IMAGE2D;
	myDescO->image_width = bmWidth;
	myDescO->image_height = bmHeight;

	input = clCreateImage(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, myFormatI, myDescI, buffer, &err);
	output = clCreateImage(context, CL_MEM_WRITE_ONLY, myFormatO, myDescO, NULL, &err);
	if (!input || !output)
	{
		printf("Error: Failed to allocate device memory!\n");
		exit(1);
	}

	// Write our data set into the input array in device memory 
	size_t originst[3];
	size_t regionst[3];
	size_t  rowPitch = 0;
	size_t  slicePitch = 0;
	originst[0] = 0; originst[1] = 0; originst[2] = 0;
	regionst[0] = bmWidth; regionst[1] = bmHeight; regionst[2] = 1;
	err = clEnqueueWriteImage(commands, input, CL_TRUE, originst, regionst, rowPitch, slicePitch, buffer, 0, NULL, NULL);
	if (err != CL_SUCCESS)
	{
		printf("Error: Failed to write to source array!\n");
		exit(1);
	}

	// Set the arguments to our compute kernel
	err = 0;
	err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &input);
	err |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &output);
	if (err != CL_SUCCESS)
	{
		printf("Error: Failed to set kernel arguments! %d\n", err);
		exit(1);
	}

	// Get the maximum work group size for executing the kernel on the device
	err = clGetKernelWorkGroupInfo(kernel, device_id, CL_KERNEL_WORK_GROUP_SIZE, sizeof(local), local, NULL);
	if (err != CL_SUCCESS)
	{
		printf("Error: Failed to retrieve kernel work group info! %d\n", err);
		exit(1);
	}

	// Execute the kernel over the entire range of our 1d input data set
	// using the maximum number of work group items for this device
	global[0] = bmWidth;
	global[1] = bmHeight;
	local[0] = 1;
	local[1] = 1;
	QueryPerformanceCounter(start);
	err = clEnqueueNDRangeKernel(commands, kernel, 2, NULL, global, local, 0, NULL, NULL);
	QueryPerformanceCounter(end);
	if (err)
	{
		printf("Error: Failed to execute kernel!\n");
		//return EXIT_FAILURE;
	}

	// Wait for the command commands to get serviced before reading back results
	//
	clFinish(commands);

	// Read back the results from the device to verify the output
	//
	err = clEnqueueReadImage(commands, output, CL_TRUE, originst, regionst, rowPitch, slicePitch, results, 0, NULL, NULL);
	if (err != CL_SUCCESS)
	{
		printf("Error: Failed to read output array! %d\n", err);
		exit(1);
	}

	// Shutdown and cleanup
	//
	clReleaseMemObject(input);
	clReleaseMemObject(output);
	clReleaseProgram(program);
	clReleaseKernel(kernel);
	clReleaseCommandQueue(commands);
	clReleaseContext(context);

	return results;
}