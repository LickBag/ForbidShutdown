#pragma once

// MAX_PATH 太短了, 在启用windows超长路径后可能有问题
#define MAX_PATH_NEW  512

#define SAFE_CLOSE_HANDLE(x) do{\
if((x) != nullptr && (x) != INVALID_HANDLE_VALUE)\
{\
	CloseHandle(x);\
	x = nullptr;\
}\
}while (false)


#define SAFE_CLOSE_SERVICE_HANDLE(x) do{\
if((x) != nullptr && (x) != INVALID_HANDLE_VALUE)\
{\
	CloseServiceHandle(x);\
	x = nullptr;\
}\
}while (false)

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

#define ARRAY_BYTES(x) ((_tcslen(x)) * sizeof(x[0]))

#define IF_TRUE_EXECUTE(x, y) if((x))\
{\
    (y);\
}

#define CHECK_TRUE_BREAK(x) if ((x))\
{\
    break;\
}
