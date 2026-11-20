#include <SDL3/SDL_main.h>

#include "flop/app/app.hpp"

int main(int argc, char *argv[]) {
  return flop::app::App().run(argc, argv);
}
