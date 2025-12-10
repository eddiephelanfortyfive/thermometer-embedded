#include <main/hardware/temperature_sensor.hpp>
#include <driver/gpio.h>
#include <esp_rom_sys.h>

// DS18B20 commands
static constexpr uint8_t CMD_SKIP_ROM = 0xCC;
static constexpr uint8_t CMD_CONVERT_T = 0x44;
static constexpr uint8_t CMD_READ_SCRATCHPAD = 0xBE;

TemperatureSensor::TemperatureSensor(gpio_num_t sensor_pin)
    : pin(sensor_pin) {}

bool TemperatureSensor::init() {
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT_OUTPUT_OD;
    io_conf.pin_bit_mask = (1ULL << pin);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    if (gpio_config(&io_conf) != ESP_OK) {
        return false;
    }
    releaseLine();
    esp_rom_delay_us(5);
    return reset();
}

bool TemperatureSensor::readTemperature(float& out_celsius) {
    // Start conversion
    if (!reset()) {
        return false;
    }
    writeByte(CMD_SKIP_ROM);
    writeByte(CMD_CONVERT_T);
    // Max conversion time 12-bit: 750ms
    // If parasite power were used, a strong pull-up would be required; assume normal power mode
    esp_rom_delay_us(750000);

    // Read scratchpad
    if (!reset()) {
        return false;
    }
    writeByte(CMD_SKIP_ROM);
    writeByte(CMD_READ_SCRATCHPAD);

    uint8_t data[9];
    for (int i = 0; i < 9; ++i) {
        data[i] = readByte();
    }
    // Validate CRC
    if (crc8Dallas(data, 8) != data[8]) {
        return false;
    }
    int16_t raw = static_cast<int16_t>((static_cast<int16_t>(data[1]) << 8) | data[0]);
    out_celsius = static_cast<float>(raw) / 16.0f;
    return true;
}

bool TemperatureSensor::reset() {
    // Drive low for at least 480us
    driveLow();
    esp_rom_delay_us(500);
    // Release and wait 70us
    releaseLine();
    esp_rom_delay_us(70);
    // Presence pulse: sensor pulls low for 60-240us
    bool presence = (readLine() == 0);
    // Wait remainder of timeslot
    esp_rom_delay_us(410);
    return presence;
}

void TemperatureSensor::writeBit(uint8_t bit) {
    if (bit) {
        // Write '1': pull low ~6us then release for rest of slot (~64us)
        driveLow();
        esp_rom_delay_us(6);
        releaseLine();
        esp_rom_delay_us(64);
    } else {
        // Write '0': pull low ~60us then release
        driveLow();
        esp_rom_delay_us(60);
        releaseLine();
        esp_rom_delay_us(10);
    }
}

uint8_t TemperatureSensor::readBit() {
    // Initiate read slot
    driveLow();
    esp_rom_delay_us(6);
    releaseLine();
    // Sample after ~9us
    esp_rom_delay_us(9);
    int level = readLine();
    // Wait rest of slot
    esp_rom_delay_us(55);
    return (level ? 1 : 0);
}

void TemperatureSensor::writeByte(uint8_t byte) {
    for (int i = 0; i < 8; ++i) {
        writeBit(byte & 0x01);
        byte >>= 1;
    }
}

uint8_t TemperatureSensor::readByte() {
    uint8_t val = 0;
    for (int i = 0; i < 8; ++i) {
        uint8_t b = readBit();
        val |= (b << i);
    }
    return val;
}

uint8_t TemperatureSensor::crc8Dallas(const uint8_t* data, uint8_t len) {
    uint8_t crc = 0;
    while (len--) {
        uint8_t inbyte = *data++;
        for (uint8_t i = 0; i < 8; i++) {
            uint8_t mix = (crc ^ inbyte) & 0x01;
            crc >>= 1;
            if (mix) {
                crc ^= 0x8C; // Dallas/Maxim CRC8 polynomial
            }
            inbyte >>= 1;
        }
    }
    return crc;
}

inline void TemperatureSensor::driveLow() {
    gpio_set_level(pin, 0);
}

inline void TemperatureSensor::releaseLine() {
    gpio_set_level(pin, 1);
}

inline int TemperatureSensor::readLine() const {
    return gpio_get_level(pin);
}


