#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "../../pigletvm.h"



// extern "C" int FUZZ_INIT_WITH_ARGS(int *argc, char ***argv) {
extern "C" int FUZZ_INIT() {

  // Add global setup code here - called once before fuzzing starts

  return 0; // Non-zero return values are reserved for future use.
}

extern "C" int FUZZ(uint8_t *Data, size_t Size) {

  // process fuzzer input (*Data) and setup local objects necessary to call the function under test

   interpret_result res = vm_interpret(Data);
    if (res != SUCCESS) {
        return 1;
    }
    uint64_t result_value = vm_get_result();
    printf("Result value: %" PRIu64 "\n", result_value);
    return 0;


  // reset state and free all locally allocated resources
}
