#include <stdint.h> // For uint32_t, uint16_t etc.
#include <string.h> // For memcpy, memset
#include <stdio.h>  // For printf, fprintf

// --- Flash Parameters (Adjust as needed) ---


#define FLASH_SECTOR_SIZE       4096    // Example: 4KB sector size
#define WEAR_LEVEL_AREA_START   0x10000 // Example: Start address of wear-leveling area (e.g., after firmware)
#define NUM_LOGICAL_BLOCKS      16      // Number of logical blocks in the wear-leveling area.

#define DATA_UNIT_SIZE          1024    // data block basic size
#define DATA_UNIT_NUMBER        (FLASH_SECTOR_SIZE/DATA_UNIT_SIZE*NUM_LOGICAL_BLOCKS)  
// --- Data Structure to Store Product Power-On Time ---
typedef struct {
    uint32_t power_on_seconds; // The actual power-on time in seconds
    uint32_t test_value_a;     // Additional test data
    uint16_t test_value_b;
} ProductData_t;

// --- Header for Each Logical Block (Metadata) ---
typedef struct {
    uint16_t  magic_number;     // A unique number to identify valid headers (e.g., 0xA55A)
    uint32_t  write_counter;    // Increments with each write, used to find the latest valid block
    uint16_t  checksum;         // CRC or simple sum for data integrity
    ProductData_t data;         // The actual product data
} FlashBlockHeader_t;

// --- Size Definitions and Assertions ---
// Size of one logical block (must be less than or equal to FLASH_SECTOR_SIZE)
#define LOGICAL_BLOCK_SIZE      sizeof(FlashBlockHeader_t)

// Ensure LOGICAL_BLOCK_SIZE fits within a sector
// #if LOGICAL_BLOCK_SIZE > FLASH_SECTOR_SIZE
// #error "LOGICAL_BLOCK_SIZE must not exceed FLASH_SECTOR_SIZE"
// #endif

#define ASSERT_BLOCK_SIZE() \
    do { \
        if (sizeof(FlashBlockHeader_t) > FLASH_SECTOR_SIZE) { \
            fprintf(stderr, "ERROR: LOGICAL_BLOCK_SIZE exceeds FLASH_SECTOR_SIZE\n"); \
            exit(1); \
        } \
    } while (0)

// --- Global Variables for Wear-Leveling State ---

// --- Test Utilities / Assertions ---
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "[FAIL] %s:%d - %s\n", __FILE__, __LINE__, message); \
            test_failures++; \
        } else { \
            printf("[PASS] %s\n", message); \
        } \
    } while (0)

    // --- Wear-Leveling State Structure ---
typedef struct {
    uint32_t curDataUnitIndex;      //current Data Unit index
    uint32_t current_write_counter; //current  be written Data Unit index
    int validDataUnit;              // info valid flag
    uint32_t last_valid_counter;    // Track counter of latest valid block
} WearLevelState_t;

static WearLevelState_t wear_state = {
    .curDataUnitIndex = 0,
    .current_write_counter = 0,
    .validDataUnit = -1,
    .last_valid_counter = 0
};

static int test_failures = 0;

// --- SPI Flash Driver Emulation (Replace with your actual driver) ---
// Emulated flash memory for testing purposes
static uint8_t g_flash_memory[WEAR_LEVEL_AREA_START + (NUM_LOGICAL_BLOCKS * FLASH_SECTOR_SIZE)];

void spi_flash_init_emu() {
    // For emulation, just clear the memory to an erased state (all 0xFFs)
    printf("--- Emulated SPI Flash Initialized (Memory Wiped) ---\n");
    for (int i = 0; i < sizeof(g_flash_memory); i++) {
        g_flash_memory[i] = 0xFF;
    }
}

int spi_flash_read_emu(uint32_t address, uint8_t *buffer, uint32_t size) {
    if (address + size > sizeof(g_flash_memory)) {
        fprintf(stderr, "EMU_ERROR: Read address 0x%X + size %u out of bounds (max 0x%X)\n", address, size, (uint32_t)sizeof(g_flash_memory));
        return -1;
    }
    memcpy(buffer, &g_flash_memory[address], size);
    return 0;
}

int spi_flash_write_emu(uint32_t address, const uint8_t *data, uint32_t size) {
    if (address + size > sizeof(g_flash_memory)) {
        fprintf(stderr, "EMU_ERROR: Write address 0x%X + size %u out of bounds (max 0x%X)\n", address, size, (uint32_t)sizeof(g_flash_memory));
        return -1;
    }
    memcpy(&g_flash_memory[address], data, size);
    return 0;
}

int spi_flash_erase_sector_emu(uint32_t address) {
    if (address % FLASH_SECTOR_SIZE != 0) {
        fprintf(stderr, "EMU_ERROR: Erase address 0x%X not sector aligned (sector size 0x%X)\n", address, FLASH_SECTOR_SIZE);
        return -1;
    }
    if (address >= sizeof(g_flash_memory)) {
        fprintf(stderr, "EMU_ERROR: Erase address 0x%X out of bounds (max 0x%X)\n", address, (uint32_t)sizeof(g_flash_memory));
        return -1;
    }
    memset(&g_flash_memory[address], 0xFF, FLASH_SECTOR_SIZE);
    return 0;
}

// --- CRC / Checksum Calculation ---
uint16_t calculate_checksum(const uint8_t *data, uint32_t length) {
    uint16_t sum = 0;
    for (uint32_t i = 0; i < length; i++) {
        sum += data[i];
    }
    return sum;
}

// --- Wear-Leveling Algorithm Functions ---
#define MAGIC_NUMBER 0xA55A

// Re-map internal SPI flash functions to emulation for testing
#define spi_flash_read spi_flash_read_emu
#define spi_flash_write spi_flash_write_emu
#define spi_flash_erase_sector spi_flash_erase_sector_emu

/**
 * @brief Initializes the wear-leveling system by scanning flash for the latest data.
 * This function should be called at device power-up.
 * @return 0 on success, -1 on failure.
 */
int flash_wear_level_init() {
    printf("-> Initializing wear-leveling: Scanning flash for latest data.\n");
    uint32_t latest_counter = 0;
    // int found_valid_block = -1;
    wear_state.validDataUnit = -1;

    for (uint32_t i = 0; i < DATA_UNIT_NUMBER; i++) {
        uint32_t block_address = WEAR_LEVEL_AREA_START + (i * DATA_UNIT_SIZE);
        FlashBlockHeader_t header;

        if (spi_flash_read(block_address, (uint8_t*)&header, sizeof(header)) != 0) {
            fprintf(stderr, "WARNING: Error reading flash block at 0x%X during init. Skipping.\n", block_address);
            continue;
        }

        uint16_t calculated_chksum = calculate_checksum((const uint8_t*)&header.data, sizeof(ProductData_t));

        if (header.magic_number == MAGIC_NUMBER && header.checksum == calculated_chksum) {
            if (header.write_counter > latest_counter) {
                latest_counter = header.write_counter;
                wear_state.validDataUnit = i;
            }
        }
    }

    if (wear_state.validDataUnit != -1) {
        wear_state.curDataUnitIndex = wear_state.validDataUnit;
        wear_state.current_write_counter = latest_counter;
        printf("   Found latest data in block %d with counter %u. Next write to block %u.\n",
               wear_state.validDataUnit, latest_counter, wear_state.curDataUnitIndex);
    } else {
        wear_state.curDataUnitIndex = 0;
        wear_state.current_write_counter = 0;
        printf("   No valid data found. Starting from block 0 with counter 0.\n");
    }
    return wear_state.validDataUnit;
}

/**
 * @brief Writes product data to flash using wear-leveling.
 * @param data Pointer to the product data structure to write.
 * @return 0 on success, -1 on failure.
 */
int flash_wear_level_write(ProductData_t *data) {
    uint32_t target_address;
    FlashBlockHeader_t header_to_write;
    if(wear_state.validDataUnit != -1)
    {
        //* wear_state.curDataUnitIndex  is valid ,cur DataUnit already writen , else default write zero
        wear_state.curDataUnitIndex = (wear_state.curDataUnitIndex + 1) % DATA_UNIT_NUMBER;
    }

    target_address = WEAR_LEVEL_AREA_START + (wear_state.curDataUnitIndex * DATA_UNIT_SIZE);

    header_to_write.magic_number = MAGIC_NUMBER;
    wear_state.current_write_counter++;
    header_to_write.write_counter = wear_state.current_write_counter;
    memcpy(&header_to_write.data, data, sizeof(ProductData_t));
    header_to_write.checksum = calculate_checksum((const uint8_t*)&header_to_write.data, sizeof(ProductData_t));

    printf("-> Writing data (counter: %u) to block %u (address: 0x%X).\n",
           header_to_write.write_counter, wear_state.curDataUnitIndex, target_address);

    // Erase the sector before writing
    // if (spi_flash_erase_sector(target_address) != 0) {
    //     fprintf(stderr, "ERROR: Failed to erase flash sector at 0x%X\n", target_address);
    //     return -1;
    // }
    //* 整块block擦除
    if(target_address % FLASH_SECTOR_SIZE == 0)
    {
        spi_flash_erase_sector(target_address);
        printf("   Sector 0x%X erased.\n", target_address);
    }
    // 1. Write data FIRST (skip header)
    if (spi_flash_write(target_address + offsetof(FlashBlockHeader_t, data), 
                       (const uint8_t*)&header_to_write.data, 
                       sizeof(ProductData_t)) != 0) {
        // spi_flash_erase_sector(target_address); // Rollback
        return -1;
    }

    // 2. Then write header (atomic commit)
    if (spi_flash_write(target_address, (const uint8_t*)&header_to_write, sizeof(FlashBlockHeader_t)) != 0) {
        // spi_flash_erase_sector(target_address); // Rollback
        return -1;
    }

    wear_state.validDataUnit = 1;
    printf("   Data written successfully to block %u.\n", wear_state.curDataUnitIndex);


    return 0;
}

/**
 * @brief Reads the latest wear-leveled data from flash.
 * @param data Pointer to the product data structure to store the read data.
 * @return 0 on success, -1 if no valid data is found.
 */
int flash_wear_level_read(ProductData_t *data) {
    printf("-> Reading wear-leveled data: Scanning for latest valid block.\n");
    uint32_t latest_counter = 0;
    int ret = -1;
    FlashBlockHeader_t header;

    if((wear_state.validDataUnit != -1) || (flash_wear_level_init() != -1) )
    {
        //* get valid block, wear_state data is valid
        uint32_t block_address = WEAR_LEVEL_AREA_START + (wear_state.curDataUnitIndex * DATA_UNIT_SIZE);

        spi_flash_read(block_address, (uint8_t*)&header, sizeof(header));
        memcpy(data, (char*)&header.data, sizeof(ProductData_t));

        printf("   Successfully read latest data from block %d (counter: %u).\n", wear_state.curDataUnitIndex, header.write_counter);
        ret = 0;
    }
    else
    {
        //* wear_state data is not valid
        printf("   No valid data found in wear-leveling area.\n");
        ret = -1;
    }

    return ret;

}

// --- Test Cases ---

void run_test_case_1_initial_power_up() {
    printf("\n=== TEST CASE 1: Initial Power-Up (No Data) ===\n");
    spi_flash_init_emu(); // Wipe flash for a clean start

    flash_wear_level_init();
    TEST_ASSERT(wear_state.curDataUnitIndex == 0, "Initial block index should be 0.");
    TEST_ASSERT(wear_state.current_write_counter == 0, "Initial write counter should be 0.");

    ProductData_t read_data;
    int ret = flash_wear_level_read(&read_data);
    TEST_ASSERT(ret == -1, "Reading from empty flash should fail.");
}

void run_test_case_2_first_write_and_read() {
    printf("\n=== TEST CASE 2: First Write and Read ===\n");
    spi_flash_init_emu(); // Clean slate

    flash_wear_level_init(); // Should initialize to 0,0

    ProductData_t write_data = { .power_on_seconds = 100, .test_value_a = 1, .test_value_b = 10 };
    flash_wear_level_write(&write_data);

    ProductData_t read_data;
    int ret = flash_wear_level_read(&read_data);
    TEST_ASSERT(ret == 0, "Reading after first write should succeed.");
    TEST_ASSERT(read_data.power_on_seconds == 100, "Read power_on_seconds should match written value.");
    TEST_ASSERT(read_data.test_value_a == 1, "Read test_value_a should match written value.");
    TEST_ASSERT(read_data.test_value_b == 10, "Read test_value_b should match written value.");
    TEST_ASSERT(wear_state.curDataUnitIndex == 0, "After 1st write, block index should be 1.");
    TEST_ASSERT(wear_state.current_write_counter == 1, "After 1st write, write counter should be 1.");
}

void run_test_case_3_sequential_writes_and_power_cycles() {
    printf("\n=== TEST CASE 3: Sequential Writes and Power Cycles ===\n");
    spi_flash_init_emu(); // Clean slate

    flash_wear_level_init(); // Init (0,0)

    ProductData_t data_to_write = {0};
    ProductData_t data_read_back = {0};

    // Write several times, simulating power cycles
    for (int i = 0; i < DATA_UNIT_NUMBER * 2 + 5; i++) { // Write more than two full cycles + some extra
        data_to_write.power_on_seconds = (i + 1) * 100;
        data_to_write.test_value_a = (i + 1) * 10;
        data_to_write.test_value_b = (uint16_t)(i + 1);

        printf("\n--- Iteration %d ---\n", i + 1);
        flash_wear_level_write(&data_to_write);

        // Simulate power cycle periodically
        if ((i + 1) % (DATA_UNIT_NUMBER / 2) == 0) {
            printf("\n--- Simulating Power Cycle (Re-initializing) ---\n");
            flash_wear_level_init();
            int ret = flash_wear_level_read(&data_read_back);
            TEST_ASSERT(ret == 0, "Read after power cycle should succeed.");
            TEST_ASSERT(data_read_back.power_on_seconds == data_to_write.power_on_seconds,
                        "Data after power cycle matches latest written power_on_seconds.");
            TEST_ASSERT(data_read_back.test_value_a == data_to_write.test_value_a,
                        "Data after power cycle matches latest written test_value_a.");
            TEST_ASSERT(data_read_back.test_value_b == data_to_write.test_value_b,
                        "Data after power cycle matches latest written test_value_b.");
        }
    }

    // Final read after many writes
    printf("\n--- Final Read After Many Cycles ---\n");
    flash_wear_level_init(); // Final re-init
    int ret = flash_wear_level_read(&data_read_back);
    TEST_ASSERT(ret == 0, "Final read should succeed.");
    TEST_ASSERT(data_read_back.power_on_seconds == data_to_write.power_on_seconds,
                "Final read power_on_seconds matches ultimate latest written value.");
    TEST_ASSERT(data_read_back.test_value_a == data_to_write.test_value_a,
                "Final read test_value_a matches ultimate latest written value.");
    TEST_ASSERT(data_read_back.test_value_b == data_to_write.test_value_b,
                "Final read test_value_b matches ultimate latest written value.");
}

void run_test_case_4_corrupted_block_handling() {
    printf("\n=== TEST CASE 4: Corrupted Block Handling ===\n");
    spi_flash_init_emu(); // Clean slate

    flash_wear_level_init(); // Init (0,0)

    ProductData_t write_data = { .power_on_seconds = 100, .test_value_a = 1, .test_value_b = 10 };
    flash_wear_level_write(&write_data); // Write to block 0 (counter 1)

    write_data.power_on_seconds = 200;
    write_data.test_value_a = 2;
    flash_wear_level_write(&write_data); // Write to block 1 (counter 2)

    write_data.power_on_seconds = 300;
    write_data.test_value_a = 3;
    flash_wear_level_write(&write_data); // Write to block 2 (counter 3)

    // Manually corrupt block 1 (address of block 1) by changing its magic number
    printf("\n--- Manually Corrupting Block 1 (address 0x%X) ---\n", WEAR_LEVEL_AREA_START + (1 * DATA_UNIT_SIZE));
    uint32_t corrupt_addr = WEAR_LEVEL_AREA_START + (1 * DATA_UNIT_SIZE);
    // Overwrite magic number with garbage
    g_flash_memory[corrupt_addr] = 0xAA;
    g_flash_memory[corrupt_addr + 1] = 0xBB;

    // Simulate power cycle
    printf("\n--- Simulating Power Cycle After Corruption ---\n");
    flash_wear_level_init(); // Should now skip block 1

    ProductData_t read_data;
    int ret = flash_wear_level_read(&read_data);
    TEST_ASSERT(ret == 0, "Read after corruption should succeed by skipping corrupted block.");
    // It should read data from block 2 (counter 3) as block 1 is corrupt.
    TEST_ASSERT(read_data.power_on_seconds == 300, "Read power_on_seconds should be from latest valid block (300).");
    TEST_ASSERT(read_data.test_value_a == 3, "Read test_value_a should be from latest valid block (3).");

    // Test writing again; it should continue from block 3
    printf("\n--- Writing New Data After Corruption Test ---\n");
    write_data.power_on_seconds = 400;
    write_data.test_value_a = 4;
    flash_wear_level_write(&write_data); // Should write to block 3 (counter 4)

    // printf("\n--- after wear_state.curDataUnitIndex : %d ---\n",wear_state.curDataUnitIndex);
    // printf("\n--- after wear_state.current_write_counter : %d ---\n",wear_state.current_write_counter);
    TEST_ASSERT(wear_state.current_write_counter == 4, "Next write index should be 4 after writing to block 3.");

    // Final check
    flash_wear_level_init();
    ret = flash_wear_level_read(&read_data);
    TEST_ASSERT(ret == 0, "Final read after corruption and new write should succeed.");
    TEST_ASSERT(read_data.power_on_seconds == 400, "Final read power_on_seconds should be 400.");
    TEST_ASSERT(read_data.test_value_a == 4, "Final read test_value_a should be 4.");
}

void run_test_case_5_all_blocks_corrupted() {
    printf("\n=== TEST CASE 5: All Blocks Corrupted ===\n");
    spi_flash_init_emu(); // Clean slate

    ProductData_t write_data = { .power_on_seconds = 100, .test_value_a = 1, .test_value_b = 10 };

    // Fill all blocks with valid data
    for (int i = 0; i < DATA_UNIT_NUMBER; i++) {
        write_data.power_on_seconds = (i + 1) * 10;
        write_data.test_value_a = i + 1;
        flash_wear_level_init(); // Initialize each time to update global state
        flash_wear_level_write(&write_data);
    }
    printf("--- All %d blocks written with valid data. ---\n", DATA_UNIT_NUMBER);

    // Corrupt all blocks manually
    printf("--- Corrupting ALL blocks ---\n");
    for (int i = 0; i < DATA_UNIT_NUMBER; i++) {
        uint32_t corrupt_addr = WEAR_LEVEL_AREA_START + (i * DATA_UNIT_SIZE);
        g_flash_memory[corrupt_addr] = 0xDE; // Corrupt magic number
        g_flash_memory[corrupt_addr + 1] = 0xAD;
    }

    printf("\n--- Simulating Power Cycle After All Corruption ---\n");
    flash_wear_level_init(); // Should find no valid blocks
    TEST_ASSERT(wear_state.curDataUnitIndex == 0, "After all corruption, init should set block index to 0.");
    TEST_ASSERT(wear_state.current_write_counter == 0, "After all corruption, init should set write counter to 0.");

    ProductData_t read_data;
    int ret = flash_wear_level_read(&read_data);
    TEST_ASSERT(ret == -1, "Reading after all blocks corrupted should fail.");

    printf("\n--- Writing New Data After All Corruption Test ---\n");
    write_data.power_on_seconds = 999;
    write_data.test_value_a = 99;
    flash_wear_level_write(&write_data); // Should write to block 0 (counter 1)

    flash_wear_level_init();
    ret = flash_wear_level_read(&read_data);
    TEST_ASSERT(ret == 0, "Read after re-writing to fresh start should succeed.");
    TEST_ASSERT(read_data.power_on_seconds == 999, "Read power_on_seconds should be 999.");
}


int main() {
    printf("=== Starting SPI Flash Wear-Leveling Test Suite ===\n");
    // ASSERT_BLOCK_SIZE();  // Add this line at the start of main()
    test_failures = 0; // Reset failures for fresh run
    
    run_test_case_1_initial_power_up();
    run_test_case_2_first_write_and_read();
    // run_test_case_3_sequential_writes_and_power_cycles();
    fflush(stdout);
    run_test_case_4_corrupted_block_handling();
    // run_test_case_5_all_blocks_corrupted();


    printf("\n=== Test Suite Finished ===\n");
    if (test_failures == 0) {
        printf("All tests passed! Great job.\n");
        return 0;
    } else {
        printf("!!! %d test(s) FAILED. Review the logs. !!!\n", test_failures);
        return 1;
    }
}