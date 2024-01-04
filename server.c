#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>
//#include <pthread.h>

#pragma comment(lib, "Ws2_32.lib")

#define PORT 8080
#define BUFFER_SIZE 1024
#define ID_SEND_BUTTON 101
#define ID_CHAT_BOX 102
#define ID_MESSAGE_EDIT 103
#define WM_UPDATE_CHAT (WM_USER + 1)

SOCKET new_socket;
HWND hChatBox, hMessageEdit;

void monoalphabeticEncrypt(char *message) {
    char key[] = "ZYXWVUTSRQPONMLKJIHGFEDCBA"; // Corrected key
    while (*message) {
        if (*message >= 'a' && *message <= 'z') {
            *message = key[*message - 'a'];
        } else if (*message >= 'A' && *message <= 'Z') {
            *message = key[*message - 'A'] - 'A' + 'a'; // Keep case consistency
        }
        message++;
    }
}

void monoalphabeticDecrypt(char *message) {
    char key[] = "ZYXWVUTSRQPONMLKJIHGFEDCBA";
    char reverse_key[26];
    for (int i = 0; i < 26; ++i) {
        reverse_key[key[i] - 'A'] = 'A' + i;
    }

    while (*message) {
        if (*message >= 'a' && *message <= 'z') {
            *message = reverse_key[*message - 'a'] - 'A' + 'a';
        } else if (*message >= 'A' && *message <= 'Z') {
            *message = reverse_key[*message - 'A'];
        }
        message++;
    }
}

void appendTextToChatBox(HWND hChatBox, const char *text) {
    int len = GetWindowTextLength(hChatBox);
    SendMessage(hChatBox, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessage(hChatBox, EM_REPLACESEL, 0, (LPARAM)text);
}

DWORD WINAPI listenForMessages(LPVOID lpParam) {
    HWND hWnd = (HWND)lpParam;
    char buffer[BUFFER_SIZE];
    int recv_size;
    while ((recv_size = recv(new_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        buffer[recv_size] = '\0'; // Null-terminate the received string
        monoalphabeticDecrypt(buffer); // Decrypt the message

        // Send message to the main window to update the chat box
        SendMessage(hWnd, WM_UPDATE_CHAT, 0, (LPARAM)strdup(buffer));
    }
    return 0;
}

LRESULT CALLBACK WindowsProcedure(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case ID_SEND_BUTTON:
                    char buffer[BUFFER_SIZE];
                    GetWindowText(hMessageEdit, buffer, BUFFER_SIZE);

                    if (strlen(buffer) > 0) {
                        monoalphabeticEncrypt(buffer);
                        send(new_socket, buffer, strlen(buffer), 0);
                        monoalphabeticDecrypt(buffer);
                        char chatMessage[BUFFER_SIZE + 20];
                        sprintf(chatMessage, "You: %s\n", buffer);
                        appendTextToChatBox(hChatBox, chatMessage);

                        SetWindowText(hMessageEdit, "");
                    }
                    break;
            }
            break;
    case WM_UPDATE_CHAT:
    char *receivedMsg = (char *)lParam;
    if (receivedMsg) {
        char chatMessage[BUFFER_SIZE + 20];
        sprintf(chatMessage, "Client: %s\n", receivedMsg);
        appendTextToChatBox(hChatBox, chatMessage);
        free(receivedMsg); // Free the duplicated string
    }
    break;
        case WM_CLOSE:
            closesocket(new_socket);
            WSACleanup();
            DestroyWindow(hWnd);
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

int main() {
    WSADATA wsaData;
    SOCKET server_fd;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    WSAStartup(MAKEWORD(2, 2), &wsaData);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 3);
    new_socket = accept(server_fd, (struct sockaddr *)&address, (int *)&addrlen);

    // Create the GUI window
    HINSTANCE hInstance = GetModuleHandle(NULL);
    WNDCLASS wc = { 0 };
    wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hInstance = hInstance;
    wc.lpszClassName = TEXT("myWindowClass");
    wc.lpfnWndProc = WindowsProcedure;
    wc.style = CS_HREDRAW | CS_VREDRAW;

    RegisterClass(&wc);

    HWND hWnd = CreateWindow(TEXT("myWindowClass"), TEXT("Server"), WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 300, NULL, NULL, hInstance, NULL);

    // Create chat box and message edit box
    hChatBox = CreateWindow(TEXT("EDIT"), NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
        10, 10, 360, 200, hWnd, (HMENU)ID_CHAT_BOX, hInstance, NULL);

    hMessageEdit = CreateWindow(TEXT("EDIT"), NULL, WS_CHILD | WS_VISIBLE | ES_AUTOVSCROLL | ES_MULTILINE,
        10, 220, 260, 30, hWnd, (HMENU)ID_MESSAGE_EDIT, hInstance, NULL);

    CreateWindow(TEXT("BUTTON"), TEXT("Send"), WS_CHILD | WS_VISIBLE,
        280, 220, 80, 30, hWnd, (HMENU)ID_SEND_BUTTON, hInstance, NULL);

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    // Create a thread for listening to incoming messages
    CreateThread(NULL, 0, listenForMessages, hWnd, 0, NULL);

    MSG msg = { 0 };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    closesocket(server_fd);
    WSACleanup();

    return 0;
}
