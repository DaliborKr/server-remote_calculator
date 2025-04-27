/**
  IPK - Project 2 - IOTA: Server for Remote Calculator
  
  Server for the IPK Calculator Protocol, which is able to communicate with
  any client using IPKCP via TCP (textual variant) or UDP (binary variant).


  Copyright (C) 2023  Dalibor Kříčka (xkrick01)

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/



#include <iostream>
#include <string.h>
#include <regex>
#include <sys/socket.h> //avaiable only on UNIX systems (more in CHANGELOG.md in section Limitations)
#include <arpa/inet.h>  //avaiable only on UNIX systems (more in CHANGELOG.md in section Limitations)
#include <signal.h>
#include <unistd.h>

using namespace std;

#define RET_CODE_ERR 1
#define RET_CODE_OK 0

#define REQUIRED_NUM_ARGS 7 // ipkcpd -h <host> -p <port> -m <mode>
#define BUFFER_SIZE 1024
#define UDP_DATA_PAYLOAD_OFFSET 2

#define MAX_CONNECTIONS 10

//Global variables
int client_sockets[MAX_CONNECTIONS];
int socket_server;
bool isTcp = false;

string non_term_expr(string *expression);


/**
 * @brief Handles interrupt signal (ctrl+c)
 * 
 * @param signum type of the signal
 */
void interrupt_signal_handler(int signum){
    if (isTcp){
        for (int i = 0; i < MAX_CONNECTIONS; i++){  
            if(client_sockets[i] != 0){
                string message = "BYE\n";
                int bytes_tx = send(client_sockets[i], message.c_str(), message.size(), 0);
                if (bytes_tx <= 0)
                    perror("ERROR: send has failed\n");
                shutdown(client_sockets[i], SHUT_RDWR);
                close(client_sockets[i]);  
            }  
        }
        shutdown(socket_server, SHUT_RDWR);
    }
    close(socket_server);
    exit(RET_CODE_OK);
}


/**
 * @brief Prints help for the program
 */
void print_help()
{
    cout << "NAME:\n"
         << "  ipkcpd - Server communicating via TCP/UDP\n"
         << "\n"
         << "USAGE:\n"
         << "  ipkcpd [options] [arguments ...]\n"
         << "\n"
         << "OPTIONS:\n"
         << "  --help\tdisplay how to use program and exit\n"
         << "  -h <VALUE>\thost IPv4 address to connect to\n"
         << "  -p <MODE>\tcommunication mode to use [possible values: tcp, udp]\n"
         << "  -m <INT>\thost port number to connect to\n"
         << "\n"
         << "AUTHOR:\n"
         << "  Dalibor Kříčka (xkrick01)\n\n";

    exit(RET_CODE_OK);
}


/**
 * @brief Validates given program arguments
 *
 * @param argc number of arguments
 * @param argv array of given arguments
 * @param mode mode, that will be used for communication (UDP/TCP)
 * @param address IPv4 address, that server is listening on
 * @param port port, that server is listening on
 */
void check_program_args(int argc, char *argv[], char *mode[], char *address[], int *port)
{
    if (argc == 2 && !strcmp(argv[1], "--help"))
    {
        print_help();
    }

    if (argc != REQUIRED_NUM_ARGS)
    {
        fprintf(stderr, "ERR: invalid number of program arguments\n");
        exit(RET_CODE_ERR);
    }

    bool addrChecked = false;
    bool portChecked = false;
    bool modeChecked = false;

    for (int i = 1; i < (REQUIRED_NUM_ARGS - 1); i++)
    {
        // check -h argument
        if ((strcmp(argv[i], "-h") == 0) && !addrChecked)
        {
            addrChecked = true;
            i++;

            // check IPv4 addres format
            if (!(regex_match(argv[i], regex("^(\\d{1,3}\\.){3}\\d{1,3}$"))))
            {
                fprintf(stderr, "ERR: invalid format of IPv4 address\n");
                exit(RET_CODE_ERR);
            }
            *(address) = argv[i];
        }
        // check -p argument
        else if ((strcmp(argv[i], "-p") == 0) && !portChecked)
        {
            portChecked = true;
            i++;

            // check port format
            if (!(regex_match(argv[i], regex("^\\d+$"))))
            {
                fprintf(stderr, "ERR: invalid format of port\n");
                exit(RET_CODE_ERR);
            }
            *(port) = atoi(argv[i]);
        }
        // check -m argument
        else if ((strcmp(argv[i], "-m") == 0) && !modeChecked)
        {
            modeChecked = true;
            i++;

            // check mode format
            if (!(regex_match(argv[i], regex("^udp|tcp$"))))
            {
                fprintf(stderr, "ERR: invalid mode (tcp or udp)\n");
                exit(RET_CODE_ERR);
            }
            *(mode) = argv[i];
        }
        else
        {
            fprintf(stderr, "ERR: invalid arguments (the server is started using: 'ipkcpd -h <host> -p <port> -m <mode>')\n");
            exit(RET_CODE_ERR);
        }
    }
}


/**
 * @brief Creates server socket
 *
 * @param mode mode, that will be used for communication (UDP/TCP)
 */
void create_socket(char *mode)
{
    int type;

    if (strcmp(mode, "udp") == 0)
    {
        type = SOCK_DGRAM; // datagram socket (UDP)
    }
    else
    {
        type = SOCK_STREAM; // stream socket (TCP)
    }

    socket_server = socket(AF_INET, type, 0);

    if (socket_server <= 0)
    {
        fprintf(stderr, "ERR: socket completion has failed\n");
        exit(RET_CODE_ERR);
    }

    if (strcmp(mode, "tcp") == 0)
    {
        int optval = 1;
        if (setsockopt(socket_server, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
        {
            perror("ERR: setsockopt has failed\n");
            exit(RET_CODE_ERR);
        }
    }
}


/**
 * @brief Sets informations about connection
 *
 * @param ip_address IPv4 address, that server is listening on
 * @param port port, that server is listening on
 * @return structure describing internet socket address
 */
struct sockaddr_in set_server_informations(char *ip_address, int port)
{
    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address)); // initialization of the memory
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = inet_addr(ip_address);

    if (bind(socket_server, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        close(socket_server);
        perror("ERR: bind has failed\n");
        exit(EXIT_FAILURE);
    }

    return server_address;
}


/**
 * @brief Applies a given operation on 2 integers
 *
 * @param operand1 first insteger operand
 * @param operand2 second insteger operand
 * @param operator math operator ( + | - | * | / )
 * @return result of the operation
 */
string calculate(string operand1, string operand2, string oper)
{
    int op1 = stoi(operand1);
    int op2 = stoi(operand2);
    int result;
    switch (oper[0])
    {
    case '+':
        result = op1 + op2;
        break;
    case '-':
        result = op1 - op2;
        break;
    case '*':
        result = op1 * op2;
        break;
    case '/':
        if (op2 == 0){      //zero division
            return "err";
        }
        else{
            result = op1 / op2;
        }
        break;
    }
    return to_string(result);
}


/**
 * @brief Function standing for non-term <operator> in LL(1) grammar (see grammar in README.md)
 *
 * @param expression expression, that is being parsed 
 * @return operator or error
 */
string non_term_operator(string *expression)
{
    if ((*expression)[0] == '+')
    {
        (*expression).erase(0, 1);
        return "+";
    }
    else if ((*expression)[0] == '-')
    {
        (*expression).erase(0, 1);
        return "-";
    }
    else if ((*expression)[0] == '*')
    {
        (*expression).erase(0, 1);
        return "*";
    }
    else if ((*expression)[0] == '/')
    {
        (*expression).erase(0, 1);
        return "/";
    }
    else
    {
        return "err";
    }
}


/**
 * @brief Function standing for non-term <opt_expr> in LL(1) grammar (see grammar in README.md)
 *
 * @param expression expression, that is being parsed
 * @param sub_result so far parsed result
 * @param oper math operator ( + | - | * | / )
 * @return ongoing result of the given math expression
 */
string non_term_optional_expr(string *expression, string sub_result, string oper){
    string operandN = "";
    string result = "";
    
    if ((*expression)[0] == ' '){
        (*expression).erase(0, 1);

        operandN = non_term_expr(expression);
        if (operandN == "err")
            return "err";

        string sub_result_new = calculate(sub_result, operandN, oper);

        result = non_term_optional_expr(expression, sub_result_new, oper);
        if (result == "err")
            return "err";

        return result;
    }
    else if ((*expression)[0] == ')'){
        return sub_result;
    }
    else{
        return "err";
    }
}


/**
 * @brief Function standing for non-term <expr> in LL(1) grammar (see grammar in README.md)
 *
 * @param expression expression, that is being parsed 
 * @return result of the given math expression/sub-expression
 */
string non_term_expr(string *expression)
{
    string operand1 = "";
    string operand2 = "";
    string operation = "";
    string result = "";

    if ((*expression)[0] == '(')
    {
        (*expression).erase(0, 1);

        operation = non_term_operator(expression);
        if (operation == "err")
            return "err";

        if ((*expression)[0] != ' ')
            return "err";
        (*expression).erase(0, 1);

        operand1 = non_term_expr(expression);
        if (operand1 == "err")
            return "err";

        if ((*expression)[0] != ' ')
            return "err";
        (*expression).erase(0, 1);

        operand2 = non_term_expr(expression);
        if (operand2 == "err")
            return "err";

        string sub_result = calculate(operand1, operand2, operation);
        result = non_term_optional_expr(expression, sub_result, operation);
        if (result == "err")
            return "err";

        if ((*expression)[0] != ')')
            return "err";
        (*expression).erase(0, 1);

        return result;
    }

    else if ((*expression)[0] >= '0' && (*expression)[0] <= '9')
    {
        while ((*expression)[0] >= '0' && (*expression)[0] <= '9')
        {
            operand1 += (*expression)[0];
            (*expression).erase(0, 1);
        }
        if ((*expression)[0] == ' ' || (*expression)[0] == ')' || (*expression)[0] == '\0' || (*expression)[0] == '\n')
        {
            return operand1;
        }
        else
        {
            return "err";
        }
    }
    else
    {
        return "err";
    }
}

/**
 * @brief Formats and returns math expression for UDP communication
 *
 * @param buffer_str expression, that was given by client
 * @return formated result of the given math expression for UDP
 */
string parse_mathe_example_udp(string buffer_str)
{
    string message = non_term_expr(&buffer_str);
    if (buffer_str[0] != '\0' && buffer_str[0] != '\n'){
        message = "err";
    }
    if (message == "err")
    {
        message = "Could not parse the message";
        message = (char)message.length() + message; // set message opcode
        message = '\x01' + message;
        message = '\x01' + message;
    }
    else
    {
        if (message[0] == '-'){
            message.erase(0, 1);
        }

        message = (char)message.length() + message; // set message opcode
        message = '\x00' + message;
        message = '\x01' + message;
    }

    return message;
}


/**
 * @brief Transform clients UDP payload data into string
 *
 * @param buffer payload data in 'char*' datatype
 * @return string of UDP payload data
 */
string parse_udp_request_data(char *buffer)
{
    if (buffer[0] != '\x00')
    {
        perror("parsing math example - not a request");
        exit(EXIT_FAILURE);
    }

    string request_data = "";
    for (int i = UDP_DATA_PAYLOAD_OFFSET; i < buffer[1] + UDP_DATA_PAYLOAD_OFFSET; i++)
    {
        request_data += buffer[i];
    }
    return request_data;
}


/**
 * @brief Formats and returns math expression for TCP communication
 *
 * @param buffer_str expression, that was given by client
 * @return formated result of the given math expression for TCP
 */
string parse_mathe_example_tcp(string buffer_str)
{   
    string message = non_term_expr(&buffer_str);
    if (buffer_str[0] != '\0' && buffer_str[0] != '\n'){
        message = "err";
    }
    if (message[0] == '-'){
        message.erase(0, 1);
    }
    return message;
}


/**
 * @brief Handles UDP communication with multiple clients
 *
 * @param address server address informations
 * @param address_size size of the server address informations
 */
void communicate_via_udp(struct sockaddr *address, socklen_t address_size)
{
    char buffer[BUFFER_SIZE];
    string message;

    while (true)
    {
        int bytes_rx = recvfrom(socket_server, buffer, BUFFER_SIZE, 0, address, &address_size);
        if (bytes_rx < 0)
            perror("ERROR: recvfrom has failed\n");

        string buffer_str = parse_udp_request_data(buffer);
        message = parse_mathe_example_udp(buffer_str);

        int bytes_tx = sendto(socket_server, message.c_str(), message.size(), 0, address, address_size);
        if (bytes_tx < 0)
            perror("ERROR: sendto has failed\n");
    }
}

/**
 * @brief Handles TCP communication with multiple clients
 *
 * @param address server address informations
 * @param address_size size of the server address informations
 */
void communicate_via_tcp(struct sockaddr *address, socklen_t address_size)
{
    //array with information if the connection with the client is in the established state (HELLO was already recieved)
    bool has_greeted_array[MAX_CONNECTIONS];

    //client sockets initialization
    for (int i = 0; i < MAX_CONNECTIONS; i++){  
        client_sockets[i] = 0;
        has_greeted_array[i] = false;
    }

    char buffer[BUFFER_SIZE];
    string message;

    if (listen(socket_server, MAX_CONNECTIONS) < 0){
        perror("ERROR: listen has failed\n");
        exit(RET_CODE_ERR);
    }

    while (true){   
        fd_set read_sockets;
        FD_ZERO(&read_sockets);
        FD_SET(socket_server, &read_sockets);
        int max_socket_descriptor = socket_server;

        //adding client sockets to file descriptor set
        for ( int i = 0 ; i < MAX_CONNECTIONS ; i++)  
        {
            if(client_sockets[i] > 0)  
                FD_SET(client_sockets[i], &read_sockets);  
        
            if(client_sockets[i] > max_socket_descriptor)  
                max_socket_descriptor = client_sockets[i];  
        }

        select(max_socket_descriptor + 1 , &read_sockets , NULL , NULL , NULL);

        //handle new client
        if (FD_ISSET(socket_server, &read_sockets)){  
            int socket_communicate = accept(socket_server, address, &address_size);
            if (socket_communicate > 0){
                for (int i = 0; i < MAX_CONNECTIONS; i++){  
                    if( client_sockets[i] == 0 ){  
                        client_sockets[i] = socket_communicate;  
                        break;  
                    }  
                } 
            }
        }

        //handle new request message from client
        for (int i = 0; i < MAX_CONNECTIONS; i++)  
        {                   
            if (FD_ISSET(client_sockets[i], &read_sockets))  
            {  
                bzero(buffer, BUFFER_SIZE);
                int bytes_rx = recv(client_sockets[i], buffer, BUFFER_SIZE, 0);
                if (bytes_rx <= 0)
                    perror("ERROR: recv has failed\n");

                message = buffer;

                //send HELLO
                if (message == "HELLO\n" && has_greeted_array[i] == false){
                    int bytes_tx = send(client_sockets[i], message.c_str(), message.size(), 0);
                    if (bytes_tx <= 0)
                        perror("ERROR: send has failed\n");
                    has_greeted_array[i] = true;    
                    continue;
                }

                //send RESULT 'int'
                if (message.find("SOLVE ", 0) != string::npos && has_greeted_array[i] == true){
                    message = parse_mathe_example_tcp(message.erase(0, 6));

                    if (message != "err"){
                        message = "RESULT " + message;
                        message = message + "\n";
                        int bytes_tx = send(client_sockets[i], message.c_str(), message.size(), 0);
                        if (bytes_tx <= 0)
                            perror("ERROR: send has failed\n");
                        continue;
                    }
                }

                //send BYE
                message = "BYE\n";
                int bytes_tx = send(client_sockets[i], message.c_str(), message.size(), 0);
                if (bytes_tx <= 0)
                    perror("ERROR: send has failed\n");
                shutdown(client_sockets[i], SHUT_RDWR);
                close(client_sockets[i]);  
                client_sockets[i] = 0;
                has_greeted_array[i] = false;
            }  
        }

    }
}

int main(int argc, char *argv[])
{
    signal(SIGINT, interrupt_signal_handler);

    char *mode;
    char *server_ip_address;
    int server_port;

    check_program_args(argc, argv, &mode, &server_ip_address, &server_port);

    create_socket(mode);
    set_server_informations(server_ip_address, server_port);

    struct sockaddr_in client_addr;
    socklen_t address_size = sizeof(client_addr);
    struct sockaddr *address = (struct sockaddr *)&client_addr;

    if (strcmp(mode, "udp") == 0)
    {
        isTcp = false;
        communicate_via_udp(address, address_size);
    }
    else
    {
        isTcp =true;
        communicate_via_tcp(address, address_size);
    }
}