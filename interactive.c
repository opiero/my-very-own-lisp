#include <stdio.h>
#include <stdlib.h>

#include <editline/readline.h>

int main (int argc, char** argv) {

    // Print Version and Exit Information
    puts("Lispy Version 0.0.0.0.1");
    puts("Press Ctrl+c to Exit\n");

    while(1) {

        // Output the prompt and get input
        char* input = readline("lispy> ");

        // Add input to history
        add_history(input);

        // Echo input back to user
        printf("No, you're a %s\n", input);

        // free retrieved input
        free(input);
    }

    return EXIT_SUCCESS;
}
