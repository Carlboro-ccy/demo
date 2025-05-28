#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h> // For inet_pton
#include "../include/sqlite3.h" // Adjust path as needed

// Helper function to create a socket and connect to the server
int connect_to_server(const char *ip, int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        return -1;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        perror("invalid address or address not supported");
        close(sockfd);
        return -1;
    }

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connection failed");
        close(sockfd);
        return -1;
    }
    return sockfd;
}

// Test case 1: Send a single message and check if it's saved
void test_send_single_message() {
    printf("Running test: test_send_single_message\n");
    const char *server_ip = "127.0.0.1"; // Assuming server runs on localhost
    int server_port = 0; // Replace with the actual server port used by server.c
                         // This needs to be known or passed as an argument

    // Attempt to read the port from a common configuration or assume a default
    // For a real test suite, this would be more robust.
    // Here, we'll prompt the user or use a hardcoded default if not easily determinable.
    // For now, let's assume a common port or one that can be configured.
    // If your server.c prints the port it's listening on, you could parse that,
    // or have a fixed port for testing.
    // Let's assume the server is started with a known port, e.g., 8080 for this test.
    // If server.c uses port 0 (dynamic port), this test client needs to know which port was assigned.
    // For simplicity, let's assume you run server.c with a fixed port, e.g., -p 8080

    FILE *fp = fopen("server_port.txt", "r");
    if (fp != NULL) {
        if (fscanf(fp, "%d", &server_port) != 1) {
            server_port = 0; // Reset if read fails
        }
        fclose(fp);
    }

    if (server_port == 0) {
        printf("Server port not found in server_port.txt. Please ensure server is running and listening on a known port.\n");
        printf("You might need to run the server with a specific port, e.g., ./server -p 8080, and then create server_port.txt with '8080'.\n");
        // As a fallback for this example, let's try a common default, but this is not ideal.
        server_port = 8080; // Fallback, replace with actual or make configurable
        printf("Attempting to connect to fallback port: %d\n", server_port);
    } else {
        printf("Found server port: %d\n", server_port);
    }


    int sockfd = connect_to_server(server_ip, server_port);
    if (sockfd < 0) {
        printf("Test failed: Could not connect to server.\n");
        return;
    }

    const char *message = "Hello Server from Test";
    ssize_t bytes_sent = send(sockfd, message, strlen(message), 0);
    if (bytes_sent < 0) {
        perror("send failed");
        close(sockfd);
        printf("Test failed: Could not send message.\n");
        return;
    }
    printf("Sent message: %s\n", message);

    // Give server a moment to process and save
    sleep(1); 

    // Verify data in database
    sqlite3 *db;
    int rc = sqlite3_open("temp.db", &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        close(sockfd);
        printf("Test failed: Could not open database to verify.\n");
        return;
    }

    sqlite3_stmt *stmt;
    char sql[256];
    // Search for the specific message. For more robust testing, consider unique IDs or timestamps.
    snprintf(sql, sizeof(sql), "SELECT message FROM temp WHERE message = '%s' ORDER BY id DESC LIMIT 1;", message);
    
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        close(sockfd);
        printf("Test failed: Could not prepare SQL statement to verify.\n");
        return;
    }

    int found = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *db_message = sqlite3_column_text(stmt, 0);
        if (db_message && strcmp((const char*)db_message, message) == 0) {
            found = 1;
            printf("Verification successful: Message found in database.\n");
        }
    }
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    close(sockfd);

    if (found) {
        printf("Test test_send_single_message: PASSED\n");
    } else {
        printf("Test test_send_single_message: FAILED - Message not found in database or mismatch.\n");
    }
}

// Test case 2: Test connection closing
void test_connection_close() {
    printf("Running test: test_connection_close\n");
    const char *server_ip = "127.0.0.1";
    int server_port = 0; 

    FILE *fp = fopen("server_port.txt", "r");
    if (fp != NULL) {
        if (fscanf(fp, "%d", &server_port) != 1) server_port = 0;
        fclose(fp);
    }
     if (server_port == 0) {
        server_port = 8080; // Fallback
        printf("Server port not found or invalid in server_port.txt, using fallback: %d\n", server_port);
    }


    int sockfd = connect_to_server(server_ip, server_port);
    if (sockfd < 0) {
        printf("Test failed: Could not connect to server for connection close test.\n");
        return;
    }
    printf("Connected for connection close test. Closing socket.\n");
    close(sockfd);
    // Server should handle this gracefully (e.g., remove fd from its list)
    // This test mainly checks if the client can connect and close.
    // Observing server logs for "Connection closed" would be a more thorough verification.
    printf("Test test_connection_close: PASSED (connection closed by client)\n");
}


int main(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "single") == 0) {
        test_send_single_message();
    } else if (argc > 1 && strcmp(argv[1], "close") == 0) {
        test_connection_close();
    } else {
        printf("Running all tests...\n");
        test_send_single_message();
        printf("\n");
        test_connection_close();
    }

    // Before running tests, ensure the server is running and listening on the correct port.
    // The server port can be passed via a file "server_port.txt" containing the port number.
    // Example: echo "8080" > server_port.txt
    // And run the server: ./server -p 8080 (or whatever port you choose)

    // To compile this test file (assuming sqlite3.c is in the parent directory or linked):
    // gcc test_server.c ../sqlite3.c -o test_server -I../ -lsqlite3 -lpthread -ldl (adjust paths and libs as needed)
    // Or if sqlite3 is compiled as part of server.c, you might not need to link it separately here if only using client parts.
    // However, for DB verification, sqlite3 object/library is needed.
    // A more common approach for testing is to link against the sqlite3 library.
    // gcc test_server.c -o test_server -lsqlite3 -I/path/to/sqlite/headers (if sqlite3 is installed system-wide)
    // For this project structure: gcc test_server.c ../sqlite3.c -o test_server -I.. -lpthread -ldl

    return 0;
}

/*
How to run:
1. Compile server.c:
   gcc server.c ../sqlite3.c -o server -I.. -lpthread -ldl 
   (Ensure sqlite3.c and sqlite3.h are in the parent directory relative to server.c, or adjust paths)

2. Start the server. If it listens on a dynamic port, you need to capture that port.
   If it listens on a fixed port (e.g., by using -p option), note it.
   Example: ./server -p 8080
   After starting, if the server prints the port it's listening on, you can manually create `server_port.txt`.
   Or, modify server.c to write its listening port to `server_port.txt`.

3. Create `server_port.txt` in the same directory as `test_server` executable, 
   containing the port number the server is listening on.
   Example: echo "8080" > server_port.txt

4. Compile test_server.c:
   gcc test_server.c ../sqlite3.c -o test_server -I.. -lpthread -ldl
   (Ensure sqlite3.c and sqlite3.h are in the parent directory relative to test_server.c, or adjust paths)

5. Run the tests:
   ./test_server
   or specific tests:
   ./test_server single
   ./test_server close

6. Check `temp.db` and server output for verification.
*/
