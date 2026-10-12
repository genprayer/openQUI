# Corsair: список устройств и очередь поддержки OpenQUI

Проверено 12 октября 2026 года. Это список моделей для планирования Linux-поддержки,
а не список устройств, уже управляемых приложением. Проверенные функции и точные
USB-ID находятся в [compatibility.md](compatibility.md).

## Границы списка и источники

- Официальный [реестр проверки совместимости Corsair](https://www.corsair.com/us/en/s/icue/icue-apps-beta): все **104 записи Corsair** из полученного снимка, **102 различных model ID**. Elgato исключён. Повторные USB-ID/варианты сохраняются.
- Официальный [список iCUE 4](https://help.corsair.com/hc/en-us/articles/360058142592-iCUE-iCUE-4-compatibility-list): **168 именованных моделей/вариантов**, включая старые клавиатуры, мыши и гарнитуры.
- Дополнительные гарнитуры добавлены по отдельным первичным источникам, указанным в таблице.
- Машинно читаемые факты: [corsair-device-inventory.json](corsair-device-inventory.json). Разные источники пересекаются; их количества нельзя складывать как число уникальных устройств.

Реестр относится к **iCUE Apps**, а не доказывает поддержку каждой строки в iCUE 5.
Для большинства его записей опубликована будущая дата **2026-11-01**; это дата
в реестре платформы, не дата выпуска аппаратного устройства и не дата реализации OpenQUI.
Гарнитур как отдельной категории в этом снимке реестра нет, поэтому они включены из других источников.
Категория `Other` и написание названий сохраняют опубликованные данные; предполагаемые
внутренние ID DRAM/iCUE LINK нельзя принимать за физические USB-ID без отдельной проверки.

Это полный снимок **перечисленных источников**, но не гарантированный перечень всех
исторических SKU, цветовых исполнений, раскладок и региональных ревизий Corsair.
Новые модели требуют дополнения источников. Публикация записи или наличие картинки
не добавляет устройство в allowlist команд OpenQUI.

## Что проверено сейчас

| Модель / путь | Состояние |
| --- | --- |
| K70 CORE RGB `1b1c:1bfd` | Частичная поддержка: RGB и Type; остальные ревизии не проверены |
| DARKSTAR WIRELESS `1b1c:1bb2`, связанный дочерний endpoint `1bdc` | Частичная поддержка: RGB, заряд, радиостатус; ограничения DPI/сопряжения в compatibility.md |
| VIRTUOSO через ресивер `1b1c:0a42` | Частичная поддержка: RGB, заряд, маршрутизация EQ/7.1; другие VIRTUOSO не считаются эквивалентными |
| SLIPSTREAM `1b1c:1bdc` и VIRTUOSO receiver `0a42` | Ограниченные функции ресиверов; не общий драйвер всех беспроводных устройств |

## Гарнитуры, которых ещё нет в реализации

Для строк ниже: **не реализовано / техническая поддержка OpenQUI не подтверждена**,
если в таблице явно не указан проверенный путь VIRTUOSO.

| Модель | Источник / примечание |
| --- | --- |
| Virtuoso RGB Wireless XT | [iCUE 4](https://help.corsair.com/hc/en-us/articles/360058142592-iCUE-iCUE-4-compatibility-list) |
| Virtuoso SE | [iCUE 4](https://help.corsair.com/hc/en-us/articles/360058142592-iCUE-iCUE-4-compatibility-list) |
| Virtuoso | [iCUE 4](https://help.corsair.com/hc/en-us/articles/360058142592-iCUE-iCUE-4-compatibility-list); частично проверен только путь `0a42` |
| Void USB | [iCUE 4](https://help.corsair.com/hc/en-us/articles/360058142592-iCUE-iCUE-4-compatibility-list) |
| Void Wireless | [iCUE 4](https://help.corsair.com/hc/en-us/articles/360058142592-iCUE-iCUE-4-compatibility-list) |
| Void Surround | [iCUE 4](https://help.corsair.com/hc/en-us/articles/360058142592-iCUE-iCUE-4-compatibility-list) |
| Void PRO USB | [iCUE 4](https://help.corsair.com/hc/en-us/articles/360058142592-iCUE-iCUE-4-compatibility-list) |
| Void PRO Wireless | [iCUE 4](https://help.corsair.com/hc/en-us/articles/360058142592-iCUE-iCUE-4-compatibility-list) |
| Void PRO Surround | [iCUE 4](https://help.corsair.com/hc/en-us/articles/360058142592-iCUE-iCUE-4-compatibility-list) |
| Void Elite USB | [iCUE 4](https://help.corsair.com/hc/en-us/articles/360058142592-iCUE-iCUE-4-compatibility-list) |
| Void Elite Wireless | [iCUE 4](https://help.corsair.com/hc/en-us/articles/360058142592-iCUE-iCUE-4-compatibility-list) |
| Void Elite Surround | [iCUE 4](https://help.corsair.com/hc/en-us/articles/360058142592-iCUE-iCUE-4-compatibility-list) |
| HS70 Wireless | [iCUE 4](https://help.corsair.com/hc/en-us/articles/360058142592-iCUE-iCUE-4-compatibility-list) |
| HS70 PRO Wireless | [iCUE 4](https://help.corsair.com/hc/en-us/articles/360058142592-iCUE-iCUE-4-compatibility-list) |
| HS70 Bluetooth | [iCUE 4](https://help.corsair.com/hc/en-us/articles/360058142592-iCUE-iCUE-4-compatibility-list) |
| HS45 Surround | [iCUE 4](https://help.corsair.com/hc/en-us/articles/360058142592-iCUE-iCUE-4-compatibility-list) |
| HS55 Surround | [iCUE 4](https://help.corsair.com/hc/en-us/articles/360058142592-iCUE-iCUE-4-compatibility-list) |
| HS60 Surround | [iCUE 4](https://help.corsair.com/hc/en-us/articles/360058142592-iCUE-iCUE-4-compatibility-list) |
| HS60 PRO Surround | [iCUE 4](https://help.corsair.com/hc/en-us/articles/360058142592-iCUE-iCUE-4-compatibility-list) |
| HS65 Surround | [iCUE 4](https://help.corsair.com/hc/en-us/articles/360058142592-iCUE-iCUE-4-compatibility-list) |
| HS60 Haptic | [iCUE 4](https://help.corsair.com/hc/en-us/articles/360058142592-iCUE-iCUE-4-compatibility-list) |
| HS75XB Wireless | [iCUE 4](https://help.corsair.com/hc/en-us/articles/360058142592-iCUE-iCUE-4-compatibility-list); iCUE доступен для обновления firmware, не обычного управления аудио |
| HS80 RGB | [iCUE 4](https://help.corsair.com/hc/en-us/articles/360058142592-iCUE-iCUE-4-compatibility-list) |
| HS80 RGB Wireless | [iCUE 4](https://help.corsair.com/hc/en-us/articles/360058142592-iCUE-iCUE-4-compatibility-list) |
| HS55 WIRELESS | [Документация Corsair](https://ir.corsair.com/news-releases/news-release-details/play-your-own-beat-corsair-launches-new-hs65-and-hs55-wireless/) |
| HS65 WIRELESS | [Документация Corsair](https://ir.corsair.com/news-releases/news-release-details/play-your-own-beat-corsair-launches-new-hs65-and-hs55-wireless/) |
| HS80 MAX WIRELESS | [Документация Corsair](https://help.corsair.com/hc/en-us/articles/19508600234637-HS80-HS80-MAX-Headset-FAQ) |
| VIRTUOSO MAX WIRELESS | [Документация Corsair](https://help.corsair.com/hc/en-us/articles/17101051641357-iCUE-5-Patch-Notes) |
| VOID WIRELESS V2 | [Документация Corsair](https://help.corsair.com/hc/en-us/articles/17101051641357-iCUE-5-Patch-Notes) |
| VOID v2 MAX WIRELESS | [Документация Corsair](https://www.corsair.com/us/en/p/gaming-headsets/ca-9011388-ww/void-v2-max-wireless-gaming-headset-carbon-ca-9011388-ww) |

Режим подключения имеет значение: аналоговый 3.5 мм и Bluetooth обычно не
предоставляют интерфейс управления iCUE. Это отдельно описано в
[статье Corsair](https://help.corsair.com/hc/en-us/articles/5861000431245-Are-analog-and-Bluetooth-headsets-supported-by-iCUE).
HS80 MAX использует другую радиосистему и не поддерживает SLIPSTREAM Multipoint;
его нельзя привязать теми же командами, что VIRTUOSO:
[FAQ HS80/HS80 MAX](https://help.corsair.com/hc/en-us/articles/19508600234637-HS80-HS80-MAX-Headset-FAQ).

## Остальные модели из списка iCUE 4

Все модели ниже пока **не реализованы / не проверены в OpenQUI**. Строки отражают
опубликованный список iCUE 4, не обещают актуальную поддержку iCUE 5.

### Keyboards

| Модель | OpenQUI |
| --- | --- |
| K100 RGB | Не реализовано / не проверено |
| K100 AIR | Не реализовано / не проверено |
| K95 | Не реализовано / не проверено |
| K95 RGB Platinum | Не реализовано / не проверено |
| K95 RGB Platinum SE | Не реализовано / не проверено |
| and K95 RGB Platinum XT | Не реализовано / не проверено |
| K70 RGB TKL | Не реализовано / не проверено |
| K70 RGB MK.2 | Не реализовано / не проверено |
| K70 MK.2 LP | Не реализовано / не проверено |
| K70 MK.2 SE | Не реализовано / не проверено |
| and K70 RGB MK.2 IG | Не реализовано / не проверено |
| K65 RGB Mini | Не реализовано / не проверено |
| K83 Wireless | Не реализовано / не проверено |
| K70 RGB | Не реализовано / не проверено |
| K70 RGB PRO | Не реализовано / не проверено |
| K70 Lux RGB | Не реализовано / не проверено |
| K70 RGB Rapidfire | Не реализовано / не проверено |
| K70 RGB SE Rapidfire | Не реализовано / не проверено |
| K70 Lux | Не реализовано / не проверено |
| K70 Rapidfire | Не реализовано / не проверено |
| K70 PRO MINI | Не реализовано / не проверено |
| Strafe | Не реализовано / не проверено |
| Strafe RGB | Не реализовано / не проверено |
| and Strafe MX | Не реализовано / не проверено |
| Strafe RGB MK.2 | Не реализовано / не проверено |
| K68 | Не реализовано / не проверено |
| K68 RGB | Не реализовано / не проверено |
| and K68 SE | Не реализовано / не проверено |
| K66 | Не реализовано / не проверено |
| K65 RGB | Не реализовано / не проверено |
| K65 Lux RGB | Не реализовано / не проверено |
| K65 RGB Rapidfire | Не реализовано / не проверено |
| K63 | Не реализовано / не проверено |
| K63 Wireless | Не реализовано / не проверено |
| K60 PRO | Не реализовано / не проверено |
| K60 RGB PRO | Не реализовано / не проверено |
| K60 RGB PRO SE | Не реализовано / не проверено |
| K60 RGB PRO LP | Не реализовано / не проверено |
| K60 PRO TKL RGB | Не реализовано / не проверено |
| K57 RGB Wireless | Не реализовано / не проверено |
| K55 RGB | Не реализовано / не проверено |
| K55 RGB PRO | Не реализовано / не проверено |
| K55 RGB PRO XT | Не реализовано / не проверено |
| K55 RGB PRO LITE | Не реализовано / не проверено |

### Mice

| Модель | OpenQUI |
| --- | --- |
| Sabre | Не реализовано / не проверено |
| Sabre RGB | Не реализовано / не проверено |
| Sabre RGB Optical | Не реализовано / не проверено |
| Sabre RGB Laser | Не реализовано / не проверено |
| Sabre Pro Champion | Не реализовано / не проверено |
| Sabre RGB Pro Champion | Не реализовано / не проверено |
| Scimitar RGB | Не реализовано / не проверено |
| Scimitar RGB Pro | Не реализовано / не проверено |
| Scimitar RGB Elite | Не реализовано / не проверено |
| Glaive RGB | Не реализовано / не проверено |
| Glaive RGB PRO | Не реализовано / не проверено |
| Dark Core RGB | Не реализовано / не проверено |
| Dark Core SE | Не реализовано / не проверено |
| Dark Core PRO | Не реализовано / не проверено |
| Dark Core PRO SE | Не реализовано / не проверено |
| M65 RGB | Не реализовано / не проверено |
| M65 RGB PRO | Не реализовано / не проверено |
| M65 RGB Elite | Не реализовано / не проверено |
| M65 RGB Elite IG | Не реализовано / не проверено |
| M65 RGB ULTRA | Не реализовано / не проверено |
| M65 RGB ULTRA WIRELESS | Не реализовано / не проверено |
| Ironclaw RGB | Не реализовано / не проверено |
| Ironclaw RGB Wireless | Не реализовано / не проверено |
| Nightsword RGB | Не реализовано / не проверено |
| Harpoon RGB | Не реализовано / не проверено |
| Harpoon RGB Wireless | Не реализовано / не проверено |
| Harpoon RGB PRO | Не реализовано / не проверено |
| M55 RGB PRO | Не реализовано / не проверено |
| Katar | Не реализовано / не проверено |
| Katar PRO | Не реализовано / не проверено |
| Katar PRO Wireless | Не реализовано / не проверено |
| Katar PRO XT | Не реализовано / не проверено |
| Katar RGB | Не реализовано / не проверено |

### Mouse mats

| Модель | OpenQUI |
| --- | --- |
| MM800 RGB Polaris | Не реализовано / не проверено |
| MM700 RGB | Не реализовано / не проверено |

### Headset stands

| Модель | OpenQUI |
| --- | --- |
| ST100 RGB | Не реализовано / не проверено |

### Power supply units

| Модель | OpenQUI |
| --- | --- |
| AX760i | Не реализовано / не проверено |
| AX860i | Не реализовано / не проверено |
| AX1200i | Не реализовано / не проверено |
| AX1500i | Не реализовано / не проверено |
| AX1600i | Не реализовано / не проверено |
| HX750i | Не реализовано / не проверено |
| HX850i | Не реализовано / не проверено |
| HX1000i | Не реализовано / не проверено |
| HX1200i | Не реализовано / не проверено |
| HX1500i | Не реализовано / не проверено |
| RM650i | Не реализовано / не проверено |
| RM750i | Не реализовано / не проверено |
| RM850i | Не реализовано / не проверено |
| RM1000i | Не реализовано / не проверено |

### Cases

| Модель | OpenQUI |
| --- | --- |
| Obsidian 1000D | Не реализовано / не проверено |
| Spec - Omega RGB | Не реализовано / не проверено |

### PC fan accessories

| Модель | OpenQUI |
| --- | --- |
| Commander PRO | Не реализовано / не проверено |
| Commander CORE | Не реализовано / не проверено |
| Commander CORE XT | Не реализовано / не проверено |
| Lighting Node PRO | Не реализовано / не проверено |
| Lighting Node CORE | Не реализовано / не проверено |

### Ambient lighting

| Модель | OpenQUI |
| --- | --- |
| LS100 Starter Kit | Не реализовано / не проверено |
| LT100 Starter Kit | Не реализовано / не проверено |
| LC100 Starter Kit | Не реализовано / не проверено |

### AIO coolers

| Модель | OpenQUI |
| --- | --- |
| H100i ELITE CAPELLIX | Не реализовано / не проверено |
| H115i ELITE CAPELLIX | Не реализовано / не проверено |
| H150i ELITE CAPELLIX | Не реализовано / не проверено |
| H100i ELITE LCD | Не реализовано / не проверено |
| H150i ELITE LCD | Не реализовано / не проверено |
| H170i ELITE LCD | Не реализовано / не проверено |
| H100i RGB ELITE | Не реализовано / не проверено |
| H115i RGB ELITE | Не реализовано / не проверено |
| H150i RGB ELITE | Не реализовано / не проверено |
| H100i PLATINUM | Не реализовано / не проверено |
| H115i PLATINUM | Не реализовано / не проверено |
| H100i PLATINUM SE | Не реализовано / не проверено |
| H100i RGB PRO XT | Не реализовано / не проверено |
| H115i RGB PRO XT | Не реализовано / не проверено |
| H150i RGB PRO XT | Не реализовано / не проверено |
| H60i RGB PRO XT | Не реализовано / не проверено |
| H115i PRO | Не реализовано / не проверено |
| H150i PRO | Не реализовано / не проверено |
| H100i PRO | Не реализовано / не проверено |
| H80i v2 | Не реализовано / не проверено |
| H80iGT | Не реализовано / не проверено |
| H100i v2 | Не реализовано / не проверено |
| H100iGTX | Не реализовано / не проверено |
| H110i | Не реализовано / не проверено |
| H110iGT | Не реализовано / не проверено |
| H115i | Не реализовано / не проверено |
| H115i GTX | Не реализовано / не проверено |

### DRAM

| Модель | OpenQUI |
| --- | --- |
| Dominator Platinum RGB | Не реализовано / не проверено |
| Vengeance LED | Не реализовано / не проверено |
| Vengeance RGB | Не реализовано / не проверено |
| Vengeance RGB PRO | Не реализовано / не проверено |
| Vengeance RGB PRO SL | Не реализовано / не проверено |
| Vengeance RGB RT | Не реализовано / не проверено |
| Vengeance RGB RS | Не реализовано / не проверено |

### Systems

| Модель | OpenQUI |
| --- | --- |
| CORSAIR ONE | Не реализовано / не проверено |
| CORSAIR ONE PRO | Не реализовано / не проверено |
| Voyager a1600 | Не реализовано / не проверено |

### Monitors

| Модель | OpenQUI |
| --- | --- |
| XENEON 32UHD144 | Не реализовано / не проверено |
| XENEON 32QHD240 | Не реализовано / не проверено |
| XENEON 32QHD165 | Не реализовано / не проверено |

## Все записи Corsair из реестра iCUE Apps

Колонка ID содержит значения **как опубликовано в реестре**. Для USB-устройств
их нужно сверить с дескриптором; для дочерних компонентов iCUE LINK/DRAM
это может быть внутренний идентификатор. Будущая дата платформы не означает,
что устройство уже поддерживается в iCUE Apps или OpenQUI.

### Fan

| Модель | Опубликованный ID | Дата в реестре Apps | OpenQUI |
| --- | --- | --- | --- |
| iCUE LINK LX RGB | `0x1C01:0x0002` | 2026-11-01 | Не реализовано / не проверено |
| iCUE LINK QX RGB | `0x1C01:0x0001` | 2026-11-01 | Не реализовано / не проверено |
| iCUE LINK RX | `0x1C01:0x0013` | 2026-11-01 | Не реализовано / не проверено |
| iCUE LINK RX MAX | `0x1C01:0x0004` | 2026-11-01 | Не реализовано / не проверено |
| iCUE LINK RX RGB | `0x1C01:0x000F` | 2026-11-01 | Не реализовано / не проверено |
| iCUE LINK RX RGB MAX | `0x1C01:0x0003` | 2026-11-01 | Не реализовано / не проверено |

### Keyboard

| Модель | Опубликованный ID | Дата в реестре Apps | OpenQUI |
| --- | --- | --- | --- |
| K100 RGB | `0x1B1C:0x1B7C` | 2026-11-01 | Не реализовано / не проверено |
| K100 RGB Air Wireless | `0x1B1C:0x1BAB` | 2026-11-01 | Не реализовано / не проверено |
| K55 CORE RGB | `0x1B1C:0x2B05` | 2026-11-01 | Не реализовано / не проверено |
| K55 CORE RGB TKL | `0x1B1C:0x2B20` | 2026-11-01 | Не реализовано / не проверено |
| K55 RGB PRO | `0x1B1C:0x1BA4` | 2026-11-01 | Не реализовано / не проверено |
| K55 RGB PRO XT | `0x1B1C:0x1BA1` | 2026-11-01 | Не реализовано / не проверено |
| K57 RGB Wireless | `0x1B1C:0x1B6E` | 2026-11-01 | Не реализовано / не проверено |
| K60 PRO | `0x1B1C:0x1B83` | 2026-11-01 | Не реализовано / не проверено |
| K60 PRO TKL RGB | `0x1B1C:0x1BC7` | 2026-11-01 | Не реализовано / не проверено |
| K60 RGB PRO | `0x1B1C:0x1BA0` | 2026-11-01 | Не реализовано / не проверено |
| K60 RGB PRO LP | `0x1B1C:0x1BAD` | 2026-11-01 | Не реализовано / не проверено |
| K60 RGB PRO SE | `0x1B1C:0x1B8D` | 2026-11-01 | Не реализовано / не проверено |
| K65 PLUS WIRELESS | `0x1B1C:0x2B37` | 2026-11-01 | Не реализовано / не проверено |
| K65 PRO MINI | `0x1B1C:0x1BD7` | 2026-11-01 | Не реализовано / не проверено |
| K65 RGB Mini | `0x1B1C:0x1BC3` | 2026-11-01 | Не реализовано / не проверено |
| K70 CORE RGB | `0x1B1C:0x1BFF` | 2026-11-01 | Эти ID не проверены; принят только `1bfd` |
| K70 CORE RGB | `0x1B1C:0x2B0A` | 2026-11-01 | Эти ID не проверены; принят только `1bfd` |
| K70 CORE RGB TKL | `0x1B1C:0x2B01` | 2026-11-01 | Не реализовано / не проверено |
| K70 CORE RGB TKL WIRELESS | `0x1B1C:0x2B02` | 2026-11-01 | Не реализовано / не проверено |
| K70 CORE SE RGB | `0x1B1C:0x2B09` | 2026-11-01 | Не реализовано / не проверено |
| K70 MAX | `0x1B1C:0x1BC0` | 2026-11-01 | Не реализовано / не проверено |
| K70 PRO RGB TKL | `0x1B1C:0x2B14` | 2026-11-01 | Не реализовано / не проверено |
| K70 RGB PRO | `0x1B1C:0x1BD4` | 2026-11-01 | Не реализовано / не проверено |
| K70 RGB PRO Mini Wireless | `0x1B1C:0x1BD3` | 2026-11-01 | Не реализовано / не проверено |
| K70 RGB TKL Champion Series | `0x1B1C:0x1BB9` | 2026-11-01 | Не реализовано / не проверено |
| K95 RGB Platinum XT | `0x1B1C:0x1B89` | 2026-11-01 | Не реализовано / не проверено |
| MAKR 75 | `0x1B1C:0x2B04` | 2026-11-01 | Не реализовано / не проверено |
| MAKR PRO 75 | `0x1B1C:0x2B1A` | 2026-11-01 | Не реализовано / не проверено |
| VANGUARD 96 | `0x1B1C:0x2B0D` | 2026-11-01 | Не реализовано / не проверено |
| VANGUARD 96 WIRELESS | `0x1B1C:0x2B0F` | 2026-11-01 | Не реализовано / не проверено |
| VANGUARD AIR 99 | `0x1B1C:0x2B21` | 2026-11-01 | Не реализовано / не проверено |
| VANGUARD PRO 96 | `0x1B1C:0x2B0E` | 2026-11-01 | Не реализовано / не проверено |

### Mouse

| Модель | Опубликованный ID | Дата в реестре Apps | OpenQUI |
| --- | --- | --- | --- |
| Dark Core PRO | `0x1B1C:0x1B80` | 2026-11-01 | Не реализовано / не проверено |
| Dark Core PRO SE | `0x1B1C:0x1B7E` | 2026-11-01 | Не реализовано / не проверено |
| DARKSTAR WIRELESS | `0x1B1C:0x1BB2` | 2026-11-01 | Частичная поддержка точного `1bb2` |
| Harpoon RGB Wireless | `0x1B1C:0x1B5E` | 2026-11-01 | Не реализовано / не проверено |
| HARPOON v2 WIRELESS | `0x1B1C:0x2B2E` | 2026-11-01 | Не реализовано / не проверено |
| HARPOON WIRED v2 | `0x1B1C:0x2B33` | 2026-11-01 | Не реализовано / не проверено |
| Ironclaw RGB Wireless | `0x1B1C:0x1B4C` | 2026-11-01 | Не реализовано / не проверено |
| IRONCLAW WIRELESS SE | `0x1B1C:0x2B32` | 2026-11-01 | Не реализовано / не проверено |
| KATAR ELITE Wireless | `0x1B1C:0x1BBA` | 2026-11-01 | Не реализовано / не проверено |
| Katar PRO | `0x1B1C:0x1B93` | 2026-11-01 | Не реализовано / не проверено |
| Katar PRO Wireless | `0x1B1C:0x1BAE` | 2026-11-01 | Не реализовано / не проверено |
| Katar PRO XT | `0x1B1C:0x1BAC` | 2026-11-01 | Не реализовано / не проверено |
| M55 | `0x1B1C:0x2B03` | 2026-11-01 | Не реализовано / не проверено |
| M55 RGB PRO | `0x1B1C:0x1B70` | 2026-11-01 | Не реализовано / не проверено |
| M55 WIRELESS | `0x1B1C:0x1BFB` | 2026-11-01 | Не реализовано / не проверено |
| M65 RGB Ultra | `0x1B1C:0x1BEF` | 2026-11-01 | Не реализовано / не проверено |
| M65 RGB Ultra Wireless | `0x1B1C:0x1BD6` | 2026-11-01 | Не реализовано / не проверено |
| M75 | `0x1B1C:0x1BF0` | 2026-11-01 | Не реализовано / не проверено |
| M75 AIR WIRELESS | `0x1B1C:0x1BF2` | 2026-11-01 | Не реализовано / не проверено |
| M75 WIRELESS | `0x1B1C:0x2B08` | 2026-11-01 | Не реализовано / не проверено |
| NIGHTSABRE WIRELESS | `0x1B1C:0x1BB8` | 2026-11-01 | Не реализовано / не проверено |
| NIGHTSWORD v2 SD | `0x1B1C:0x2B1C` | 2026-11-01 | Не реализовано / не проверено |
| Sabre PRO Champion Series | `0x1B1C:0x1B7A` | 2026-11-01 | Не реализовано / не проверено |
| Sabre RGB PRO Champion Series | `0x1B1C:0x1B79` | 2026-11-01 | Не реализовано / не проверено |
| Sabre RGB PRO Wireless | `0x1B1C:0x1B98` | 2026-11-01 | Не реализовано / не проверено |
| SCIMITAR ELITE WIRELESS SE | `0x1B1C:0x2B22` | 2026-11-01 | Не реализовано / не проверено |
| SCIMITAR RGB ELITE | `0x1B1C:0x1BE3` | 2026-11-01 | Не реализовано / не проверено |
| SCIMITAR RGB ELITE WIRELESS | `0x1B1C:0x1BDB` | 2026-11-01 | Не реализовано / не проверено |

### Other

| Модель | Опубликованный ID | Дата в реестре Apps | OpenQUI |
| --- | --- | --- | --- |
| COMMANDER DUO | `0x1B1C:0x0C56` | 2026-11-01 | Не реализовано / не проверено |
| DOMINATOR PLATINUM RGB | `0x1C00:0x0404` | 2026-11-01 | Не реализовано / не проверено |
| DOMINATOR PLATINUM RGB DDR5 | `0x1C00:0x0408` | 2026-11-01 | Не реализовано / не проверено |
| DOMINATOR TITANIUM | `0x1C00:0x040B` | 2026-11-01 | Не реализовано / не проверено |
| iCUE LINK 5000T RGB | `0x1C01:0x0205` | 2026-11-01 | Не реализовано / не проверено |
| iCUE LINK 9000D RGB AIRFLOW | `0x1C01:0x0105` | 2026-11-01 | Не реализовано / не проверено |
| iCUE LINK ADAPTER | `0x1C01:0x0005` | 2026-11-01 | Не реализовано / не проверено |
| iCUE LINK GPU RGB | `0x1C01:0x000D` | 2026-11-01 | Не реализовано / не проверено |
| iCUE LINK H100i RGB | `0x1C01:0x0407` | 2026-11-01 | Не реализовано / не проверено |
| iCUE LINK H150i RGB | `0x1C01:0x0207` | 2026-11-01 | Не реализовано / не проверено |
| iCUE LINK H150i RGB | `0x1C01:0x0507` | 2026-11-01 | Не реализовано / не проверено |
| iCUE LINK H170i RGB | `0x1C01:0x0307` | 2026-11-01 | Не реализовано / не проверено |
| iCUE LINK System Hub | `0x1B1C:0x0C3F` | 2026-11-01 | Не реализовано / не проверено |
| iCUE LINK TITAN 240 | `0x1C01:0x0011` | 2026-11-01 | Не реализовано / не проверено |
| iCUE LINK TITAN 280 | `0x1C01:0x0111` | 2026-11-01 | Не реализовано / не проверено |
| iCUE LINK TITAN 360 | `0x1C01:0x0211` | 2026-11-01 | Не реализовано / не проверено |
| iCUE LINK TITAN 420 | `0x1C01:0x0311` | 2026-11-01 | Не реализовано / не проверено |
| iCUE LINK XC7 ELITE | `0x1C01:0x0009` | 2026-11-01 | Не реализовано / не проверено |
| iCUE LINK XD5 ELITE | `0x1C01:0x000C` | 2026-11-01 | Не реализовано / не проверено |
| iCUE LINK XD6 ELITE | `0x1C01:0x0019` | 2026-11-01 | Не реализовано / не проверено |
| iCUE LINK XG3 HYBRID | `0x1C01:0x000A` | 2026-11-01 | Не реализовано / не проверено |
| K57 Harpoon RGB Wireless Bundle | `0x1B1C:0x1BB0` | 2026-11-01 | Не реализовано / не проверено |
| Nautilus LCD Cap | `0x1B1C:0x0C55` | 2026-11-01 | Не реализовано / не проверено |
| Nautilys LCD Cap | `0x1B1C:0x0C57` | 2026-11-01 | Не реализовано / не проверено |
| SHUGO SERIES DDR5 | `0x1C00:0x0A01` | 2026-11-01 | Не реализовано / не проверено |
| SLIPSTREAM v2 WIRELESS USB Receiver | `0x1B1C:0x2B1B` | 2026-11-01 | Не реализовано / не проверено |
| SLIPSTREAM WIRELESS USB Receiver | `0x1B1C:0x1BDC` | 2026-11-01 | Ограниченный проверенный путь `1bdc` |
| VENGEANCE DDR5 | `0x1C00:0x0409` | 2026-11-01 | Не реализовано / не проверено |
| VENGEANCE LED | `0x1C00:0x0401` | 2026-11-01 | Не реализовано / не проверено |
| VENGEANCE RGB | `0x1C00:0x0402` | 2026-11-01 | Не реализовано / не проверено |
| VENGEANCE RGB DDR5 | `0x1C00:0x040A` | 2026-11-01 | Не реализовано / не проверено |
| VENGEANCE RGB PRO | `0x1C00:0x0403` | 2026-11-01 | Не реализовано / не проверено |
| VENGEANCE RGB PRO SL | `0x1C00:0x0405` | 2026-11-01 | Не реализовано / не проверено |
| VENGEANCE RGB RS | `0x1C00:0x0406` | 2026-11-01 | Не реализовано / не проверено |
| VENGEANCE RGB RS DDR5 | `0x1C00:0x0B00` | 2026-11-01 | Не реализовано / не проверено |
| VENGEANCE RGB RT | `0x1C00:0x0407` | 2026-11-01 | Не реализовано / не проверено |
| VOXARA | `0x1B1C:0x2A15` | 2026-09-21 | Не реализовано / не проверено |
| VRM Cooler Module | `0x1C01:0x0010` | 2026-11-01 | Не реализовано / не проверено |

## Что требуется для новой модели

1. Получить точный VID:PID, descriptor, firmware и фактический путь подключения.
2. Добавить реальные ресурсы/метаданные и проверить геометрию выбора LED/кнопок.
3. Восстановить и проверить запросы/ответы протокола для конкретной функции.
4. Реализовать transport/backend с ограничениями, обработкой ошибок и восстановлением режима.
5. Пройти тесты с фикстурами и физическую проверку на устройстве или получить надёжные USB-захваты от владельца.
6. Обновить статус конкретной функции, не всей линейки сразу.

Подробный порядок: [adding-devices.md](adding-devices.md). Наличие записи в этом
файле не включает экспериментальные команды и не обещает срок реализации.
