﻿#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <cstdio>
#include <dlfcn.h>
#include <mutex>
#include <stdint.h>
#include <thread>

#include "../shared/ipc.cpp"
#include "../shared/swtfb.cpp"

using namespace std;
using namespace swtfb;

int msg_q_id = 0x2257c;
ipc::Queue MSGQ(msg_q_id);

extern "C" {
static void _libhook_init() __attribute__((constructor));
static void _libhook_init() { printf("LIBHOOK INIT\n"); }

int main(int, char **, char **) {
  SwtFB fb;
  uint16_t *shared_mem = ipc::get_shared_buffer();

  mutex draw_queue_m;
  vector<mxcfb_update_data> updates;

  auto th = new thread([&]() {
    while (true) {
      bool was_dirty = false;
      mxcfb_rect dirty_area;
      swtfb::reset_dirty(dirty_area);

      draw_queue_m.lock();
			auto todo = updates;
      updates.clear();
      draw_queue_m.unlock();

      for (auto update : todo) {
        auto rect = update.update_region;
        #ifdef DEBUG
				std::cerr << "Dirty Region: " << rect.left << " " << rect.top << " "
						 << rect.width << " " << rect.height << endl;
        #endif

        int mode = update.waveform_mode;
        if (mode <= 4) { mode = 1; }
        if (mode > 4) { mode = 2; }
        mode = 1;

				int size = rect.width * rect.height;
				if (size > 500 * 500) {

          cerr << "Using thread" << rect.width << " " << rect.height << endl;
          auto nt = new thread([&]() {
            fb.DrawRaw(shared_mem, rect.left, rect.top, rect.width, rect.height,
                       mode, 1);
          });

        } else {
          fb.DrawRaw(shared_mem, rect.left, rect.top, rect.width, rect.height,
                     mode, 1);
        }

      }
      usleep(1000);

    }
  });

  printf("WAITING FOR SEND UPDATE ON MSG Q\n");
  while (true) {
    mxcfb_update_data buf = MSGQ.recv();
    auto rect = buf.update_region;

    draw_queue_m.lock();
    updates.push_back(buf);
    draw_queue_m.unlock();
  }
}

int __libc_start_main(int (*_main)(int, char **, char **), int argc,
                      char **argv, int (*init)(int, char **, char **),
                      void (*fini)(void), void (*rtld_fini)(void),
                      void *stack_end) {

  printf("LIBC START HOOK\n");

  typeof(&__libc_start_main) func_main =
      (typeof(&__libc_start_main))dlsym(RTLD_NEXT, "__libc_start_main");

  return func_main(main, argc, argv, init, fini, rtld_fini, stack_end);
};
};
