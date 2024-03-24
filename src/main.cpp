#include <unistd.h>

#include <iostream>

#include "makefile-builder.h"

int main(int argc, char *argv[]) {
    MakefileBuilder makefileBuilder;
    std::string makefilePath = "";
    int concurrency = 1;
    std::vector<std::string> targets;

    int opt;
    while ((opt = getopt(argc, argv, "f:j:")) != -1) {
        switch (opt) {
            case 'f':
                makefilePath = optarg;
                break;
            case 'j':
                concurrency = std::stoi(optarg);
                break;
            default:
                std::cerr << "Usage: " << argv[0]
                          << " [-f makefile path] [-j number of targets that "
                             "can build simultaneously] [target...]\n";
                return 1;
        }
    }

    for (int i = optind; i < argc; i++) {
        targets.push_back(argv[i]);
    }

    makefileBuilder.build(makefilePath, targets, concurrency);

    return 0;
}