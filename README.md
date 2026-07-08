# MadCat OS v2.0

Firmware para ESP32 com CC1101 + nRF24 + BLE + WiFi + OLED 0.96"

## Compilação via GitHub Actions

1. Faça fork deste repositório
2. Vá em **Actions** → **Build MadCat OS v2.0 Firmware** → **Run workflow**
3. Baixe os artefatos gerados

## Arquivos Gerados (4 bins)

Após a compilação, você terá:

| Arquivo | Offset | Descrição |
|---------|--------|-----------|
| `bootloader.bin` | 0x1000 | Bootloader ESP32 |
| `partitions.bin` | 0x8000 | Tabela de partições |
| `boot_app0.bin` | 0xE000 | Boot app0 |
| `firmware.bin` | 0x10000 | Firmware MadCat OS v2.0 |

## Flash

### Linux/Mac:
```bash
./flash.sh /dev/ttyUSB0
# ou tudo de uma vez:
./flash_all.sh /dev/ttyUSB0


