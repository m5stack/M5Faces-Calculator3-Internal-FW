# M5Faces Calculator3 I2C Control Protocol

| Document information | Details |
| :--- | :--- |
| Product | M5Faces Calculator3 |
| SKU | `A005-V3` |
| Applicable firmware | `v03` |
| Interface | I2C |
| Language | English |

---

## 1. Hardware Interface

| Item | Description |
| :--- | :--- |
| Default I2C slave address (7-bit) | `0x08` |
| Recommended configurable address range | `0x08`-`0x77` |
| Bus speed | Standard Mode `100 kbps` / Fast Mode `400 kbps` |
| IRQ active level | Active low |
| Key-data length | 1 byte per read |
| Long-press threshold | Greater than `400 ms` |

### 1.1 IRQ Behavior

After a key is released and a valid key code is generated, the module pulls `IRQ` low. After the host directly reads the 1-byte key data, the module returns `IRQ` high.

```text
Key released; data generated          Host reads key data
              |                                |
--------------+ IRQ low (0) ------------------+ IRQ high (1)
```

> The firmware releases `IRQ` at the end of any I2C read transaction. When `IRQ` is low, the host should read the key data before reading a configuration register.

---

## 2. Communication Overview

The module supports three types of I2C operations:

| Operation | Host transaction | Purpose |
| :--- | :--- | :--- |
| Key-data read | Directly read 1 byte | Get the most recently generated key code and release IRQ |
| Register read | Write a 1-byte register address, then read | Get the device ID, UID, firmware version, or current I2C address |
| Command write | Write `[register address] [data]`, 2 bytes total | Enter the IAP boot flow or change the I2C address |

All multi-byte data is transmitted from left to right as shown in the tables.

---

## 3. Key-Data Read

When `IRQ` is low, the host directly reads exactly 1 byte from the module without first writing a register address. Key data is generated when the key is released.

### 3.1 Key Matrix and Returned Values

The module uses a 5-row x 4-column key matrix. A normal short press returns the single-byte ASCII code of the corresponding character.

| Row / column | Col 0 | Col 1 | Col 2 | Col 3 |
| :---: | :---: | :---: | :---: | :---: |
| Row 0 | `A` (`0x41`) | `M` (`0x4D`) | `%` (`0x25`) | `/` (`0x2F`) |
| Row 1 | `7` (`0x37`) | `8` (`0x38`) | `9` (`0x39`) | `*` (`0x2A`) |
| Row 2 | `4` (`0x34`) | `5` (`0x35`) | `6` (`0x36`) | `-` (`0x2D`) |
| Row 3 | `1` (`0x31`) | `2` (`0x32`) | `3` (`0x33`) | `+` (`0x2B`) |
| Row 4 | `.` (`0x2E`) | `0` (`0x30`) | `` ` `` (`0x60`) | `=` (`0x3D`) |

### 3.2 Long-Press Special Codes

Only `A` and `=` define special long-press behavior. When either key is held for more than `400 ms` and then released, the following control code is returned:

| Key | Short-press value | Long-press value | Long-press meaning |
| :---: | :---: | :---: | :--- |
| `A` | `0x41` | `0x08` | Backspace |
| `=` | `0x3D` | `0x0D` | Carriage Return (CR) |

Holding any other key returns its normal ASCII code.

### 3.3 Event Buffering

The module retains only one unread key byte and does not provide a key FIFO. If new key data is generated before the host reads the previous data, the previous byte may be overwritten. The host should read promptly after detecting that `IRQ` is low.

---

## 4. Register Address Map

| Register address | Readable | Writable | Default / returned value | Description |
| :--- | :---: | :---: | :--- | :--- |
| `0xD0` | Yes | No | `0x01` | Device type ID (Calculator) |
| `0xE0`-`0xEB` | Yes | No | Device-specific | 96-bit MCU UID, 12 bytes total |
| `0xFD` | No | Yes | - | System reset / IAP entry command |
| `0xFE` | Yes | No | `0x03` | Firmware version |
| `0xFF` | Yes | Yes | `0x08` | I2C address register |

> Addresses not listed above are outside the public control protocol. The host must not rely on their returned values or write behavior.

---

## 5. Register Reads

### 5.1 Read Procedure

1. The host writes a 1-byte register address to the module.
2. The host issues a repeated START or a new I2C read transaction.
3. The module returns data from the selected register block.

The host must not read more than the maximum length specified below.

### 5.2 Readable Register Blocks

#### Device Type ID

| Start address | Readable bytes | Returned data |
| :--- | :---: | :--- |
| `0xD0` | 1 | `[0x01]` |

#### MCU UID

| Start address | Maximum readable bytes | Returned data |
| :--- | :---: | :--- |
| `0xE0`-`0xEB` | `0xEC - start address` | Remaining UID data starting at the corresponding offset |

The UID is read into a cache at power-on. To read the complete UID, start at `0xE0` and read 12 bytes. The byte order follows ascending STM32 UID storage addresses.

#### Firmware Version and I2C Address

| Start address | Readable bytes | Returned data |
| :--- | :---: | :--- |
| `0xFE` | 2 | `[firmware_version, i2c_address]` |
| `0xFF` | 1 | `[i2c_address]` |

Firmware `v03` returns `0x03` as `firmware_version`. `i2c_address` is the current 7-bit slave address and does not include the read/write direction bit.

---

## 6. Command Writes

The public command write format is exactly 2 bytes:

```text
[register address] [data]
```

### 6.1 System Reset / IAP Entry (`0xFD`)

| Written data | Behavior |
| :---: | :--- |
| `0x01` | Make the module enter the bootloader / IAP boot flow |
| Any other value | No operation |

Example:

```text
[0xFD] [0x01]
```

> This document defines only the IAP entry point in the application firmware. It does not define the subsequent firmware transfer protocol.

### 6.2 Change I2C Address (`0xFF`)

| Item | Description |
| :--- | :--- |
| Recommended address range | `0x08`-`0x77` |
| Firmware-accepted range | `0x01`-`0x7F` |
| Effective time | After the write transaction completes |
| Retained after power-off | Yes, stored in internal Flash |

The I2C specification reserves `0x00`-`0x07` and `0x78`-`0x7F`. These addresses are not recommended even where the firmware accepts them.

Example: change the module address to `0x20`:

```text
[0xFF] [0x20]
```

After the write completes, the host must use the new address for further communication. Wait for the configuration write to complete and read `0xFF` at the new address to confirm the change. A written value of `0x00` or greater than `0x7F` is ignored.

---

## 7. Host Access Examples

All addresses in the following pseudocode are 7-bit I2C addresses.

### 7.1 Read Key Data

```c
uint8_t key_code;
i2c_read(device_address, &key_code, 1);
```

### 7.2 Read Firmware Version and Current Address

```c
uint8_t reg = 0xFE;
uint8_t info[2];
i2c_write(device_address, &reg, 1);
i2c_read(device_address, info, 2);
```

### 7.3 Read the Complete UID

```c
uint8_t reg = 0xE0;
uint8_t uid[12];
i2c_write(device_address, &reg, 1);
i2c_read(device_address, uid, 12);
```

---

## 8. Usage Notes

- The host should prioritize reading the 1-byte key data after detecting that `IRQ` is low.
- A key code is generated when the key is released; holding a key does not generate repeat events.
- The module returns one key code at a time. It does not provide the complete matrix state or a multi-key combination state.
- Register reads and key-data reads use different transaction flows and must not be mixed.
- After changing the I2C address, retain or rediscover the new address so the device remains reachable after subsequent startups.
