<!-- Improved compatibility of back to top link -->
<a id="readme-top"></a>

<!-- PROJECT SHIELDS -->
[![Contributors][contributors-shield]][contributors-url]
[![Forks][forks-shield]][forks-url]
[![Stargazers][stars-shield]][stars-url]
[![Issues][issues-shield]][issues-url]
[![project_license][license-shield]][license-url]

<!-- PROJECT LOGO -->
<br />
<div align="center">
  <h3 align="center">OpenAstroFocuser</h3>

  <p align="center">
    Moonlite-compatible focuser firmware built on Zephyr RTOS.
    <br />
    <a href="https://openastrotech.github.io/OpenAstroFocuser"><strong>Explore the docs »</strong></a>
    <br />
    <br />
    <a href="https://github.com/OpenAstroTech/OpenAstroFocuser">View Demo</a>
    ·
    <a href="https://github.com/OpenAstroTech/OpenAstroFocuser/issues/new?labels=bug&template=bug-report---.md">Report Bug</a>
    ·
    <a href="https://github.com/OpenAstroTech/OpenAstroFocuser/issues/new?labels=enhancement&template=feature-request---.md">Request Feature</a>
  </p>
</div>

<!-- TABLE OF CONTENTS -->
<details>
  <summary>Table of Contents</summary>
  <ol>
    <li>
      <a href="#about-the-project">About The Project</a>
      <ul>
        <li><a href="#built-with">Built With</a></li>
      </ul>
    </li>
    <li>
      <a href="#getting-started">Getting Started</a>
      <ul>
        <li><a href="#prerequisites">Prerequisites</a></li>
        <li><a href="#installation">Installation</a></li>
      </ul>
    </li>
    <li><a href="#usage">Usage</a></li>
    <li><a href="#roadmap">Roadmap</a></li>
    <li><a href="#contributing">Contributing</a></li>
    <li><a href="#license">License</a></li>
    <li><a href="#contact">Contact</a></li>
    <li><a href="#acknowledgments">Acknowledgments</a></li>
  </ol>
</details>

## About The Project

OpenAstroFocuser is built for astrophotographers and observatory tinkerers who want reliable, scriptable focusing on DIY hardware. Plug it into any Moonlite-compatible controller (real hardware or software clients such as NINA, Ekos, or ASCOM drivers) and you get:

- **Preset-aware autofocus** – store and recall absolute positions, then let your capture suite step through autofocus routines without losing calibration.
- **Live telemetry** – query current/new positions, motion state, temperature, and speed over the Moonlite serial link to feed dashboards or automation scripts.
- **Manual and automated motion** – stage moves, cancel in-flight slews, or flip between half/full-step microstepping directly from your control software.
- **Configurable speed profiles** – adjust the Moonlite delay multiplier on the fly to trade speed for torque when heavy imaging trains are attached.
- **Hardware flexibility** – run on ESP32-S3 reference hardware, custom shields, or any board with Zephyr support and a UART interface.
- **Ready-to-use documentation & tests** – follow the included docs, CI, and ztest suites to adapt the firmware to your rig with confidence.

Every release follows Zephyr `main`, so you always know which toolchain and modules were used to build the shipped firmware.

### Built With

* [![Zephyr][Zephyr.io]][Zephyr-url]
* [![CMake][CMake]][CMake-url]
* [![C++20][Cpp]][Cpp-url]
* [![ESP-IDF][Espidf]][Espidf-url]

<p align="right">(<a href="#readme-top">back to top</a>)</p>

## Getting Started

Set up the Zephyr toolchain per the [Getting Started Guide](https://docs.zephyrproject.org/latest/getting_started/index.html). The commands below assume a Linux host and Zephyr SDK.

### Prerequisites

- West (installed via `pip install west`)
- Zephyr SDK or an equivalent toolchain
- Python 3.10+

### Installation

1. Initialise a workspace pointing at this manifest:

```shell
west init -m https://github.com/OpenAstroTech/OpenAstroFocuser --mr main OpenAstroFocuser-workspace
cd OpenAstroFocuser-workspace
west update
```

2. Export the Zephyr environment (optional but handy):

   ```shell
   source zephyr/zephyr-env.sh
   ```

3. (Optional) Install Python requirements for documentation:

   ```shell
   pip install -r OpenAstroFocuser/doc/requirements.txt
   ```

<p align="right">(<a href="#readme-top">back to top</a>)</p>

## Usage

### Build & Flash

```shell
west build -b esp32s3_devkitc/esp32s3/procpu OpenAstroFocuser/app
west flash
```

Pass `-DEXTRA_CONF_FILE=debug.conf` for verbose logging or switch `-b` to any supported board/overlay.

### Run Moonlite Parser Tests

```shell
west build -b qemu_x86 OpenAstroFocuser/tests/lib/moonlite --build-dir build/moonlite_test --pristine auto
west build -t run --build-dir build/moonlite_test
```

### Twister Integration Suite

```shell
west twister -T OpenAstroFocuser/tests --integration
```

<p align="right">(<a href="#readme-top">back to top</a>)</p>

## Roadmap

- [ ] Automatic homing / end stop detection.
- [ ] Saving of last known position in EEPROM

See the [open issues](https://github.com/OpenAstroTech/OpenAstroFocuser/issues) for the full backlog.

<p align="right">(<a href="#readme-top">back to top</a>)</p>

## Contributing

Contributions keep this firmware evolving. If you have ideas or fixes:

1. Fork the repo
2. Create your feature branch (`git checkout -b feature/AmazingFocuserBoost`)
3. Commit (`git commit -m 'Add some AmazingFocuserBoost'`)
4. Push (`git push origin feature/AmazingFocuserBoost`)
5. Open a Pull Request

Please run the formatter (`west clang-format`) and the applicable tests before sending PRs.

<p align="right">(<a href="#readme-top">back to top</a>)</p>

### Top contributors

<a href="https://github.com/OpenAstroTech/OpenAstroFocuser/graphs/contributors">
  <img src="https://contrib.rocks/image?repo=OpenAstroTech/OpenAstroFocuser" alt="Contributor graph" />
</a>

## License

Distributed under the Apache License 2.0. See `LICENSE` for details.

<p align="right">(<a href="#readme-top">back to top</a>)</p>

## Contact

Project Link: [https://github.com/OpenAstroTech/OpenAstroFocuser](https://github.com/OpenAstroTech/OpenAstroFocuser)

Zephyr Discord: `#moonlite-focuser`

<p align="right">(<a href="#readme-top">back to top</a>)</p>

## Acknowledgments

* [Zephyr Project](https://zephyrproject.org)
* [Moonlite Focuser Protocol Reference](https://moonlitefocuser.com)
* [Contrib.rocks](https://contrib.rocks)

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- MARKDOWN LINKS & IMAGES -->
[contributors-shield]: https://img.shields.io/github/contributors/OpenAstroTech/OpenAstroFocuser.svg?style=for-the-badge
[contributors-url]: https://github.com/OpenAstroTech/OpenAstroFocuser/graphs/contributors
[forks-shield]: https://img.shields.io/github/forks/OpenAstroTech/OpenAstroFocuser.svg?style=for-the-badge
[forks-url]: https://github.com/OpenAstroTech/OpenAstroFocuser/network/members
[stars-shield]: https://img.shields.io/github/stars/OpenAstroTech/OpenAstroFocuser.svg?style=for-the-badge
[stars-url]: https://github.com/OpenAstroTech/OpenAstroFocuser/stargazers
[issues-shield]: https://img.shields.io/github/issues/OpenAstroTech/OpenAstroFocuser.svg?style=for-the-badge
[issues-url]: https://github.com/OpenAstroTech/OpenAstroFocuser/issues
[license-shield]: https://img.shields.io/github/license/OpenAstroTech/OpenAstroFocuser.svg?style=for-the-badge
[license-url]: https://github.com/OpenAstroTech/OpenAstroFocuser/blob/main/LICENSE
[product-screenshot]: doc/_static/moonlite_parser.png
[Zephyr.io]: https://img.shields.io/badge/Zephyr-183D4C?style=for-the-badge&logo=zephyr&logoColor=white
[Zephyr-url]: https://zephyrproject.org
[CMake]: https://img.shields.io/badge/CMake-064F8C?style=for-the-badge&logo=cmake&logoColor=white
[CMake-url]: https://cmake.org
[Cpp]: https://img.shields.io/badge/C%2B%2B20-00599C?style=for-the-badge&logo=cplusplus&logoColor=white
[Cpp-url]: https://isocpp.org
[Espidf]: https://img.shields.io/badge/ESP--IDF-E7352C?style=for-the-badge&logo=espressif&logoColor=white
[Espidf-url]: https://www.espressif.com/en/products/sdks/esp-idf
