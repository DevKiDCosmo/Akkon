#include "shell/Shell.h"

int main(int argc, char* argv[]) {
    shell::Shell app;
    return app.run(argc, argv);
}