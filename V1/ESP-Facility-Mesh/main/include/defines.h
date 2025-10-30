#define MAC_ADDR_LEN 6


typedef enum {
    NO_ROLE,
    EAST_WB,
    WEST_WB,
    EAST_GATE,
    WEST_GATE,

    //ATTENTION: ALWAYS KEEP NUM_FUNCTIONS LAST
    NUM_FUNCTIONS     
} functions;

static const char* function_names[NUM_FUNCTIONS] = {
    [NO_ROLE]   = "NO_ROLE",
    [EAST_WB]   = "EAST_WB",
    [WEST_WB]  = "WEST_WB",
    [EAST_GATE]  = "EAST_GATE",
    [WEST_GATE] = "WEST_GATE",

    // …  
};
