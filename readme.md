## Year of the Depend Adult Undergarment

[//]: # ([![codecov]&#40;https://codecov.io/gh/ninjaro/yodau/graph/badge.svg?token=TRWFFRPDMO&#41;]&#40;https://codecov.io/gh/ninjaro/yodau&#41;)
[//]: # ([![Codacy Badge]&#40;https://app.codacy.com/project/badge/Grade/bfbea7685e154a2ab670d75ffe4f3509&#41;]&#40;https://app.codacy.com/gh/ninjaro/yodau/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade&#41;)
[![license](https://img.shields.io/github/license/ninjaro/yodau?color=e6e6e6)](https://github.com/ninjaro/yodau/blob/master/license)  
[![Checks](https://github.com/ninjaro/yodau/actions/workflows/tests.yml/badge.svg)](https://github.com/ninjaro/yodau/actions/workflows/tests.yml)
[![Deploy](https://github.com/ninjaro/yodau/actions/workflows/html.yml/badge.svg)](https://github.com/ninjaro/yodau/actions/workflows/html.yml)
[![version](https://img.shields.io/github/v/release/ninjaro/yodau?include_prereleases)](https://github.com/ninjaro/yodau/releases/latest)

**Abstract.** She said she would come. I thought she'd come by nine. I pulled the Venetian blinds on the one window whose
yellow light kept spilling across the floor and making it harder to just sit and wait. I could not quite focus: even
without headsets, Lou Reed kept coming in waves, sometimes murmuring that he is waiting for a man, sometimes hitting my
right ear so hard my whole body twisted up; I pressed my palm against the ringing eardrum, unsure whether it was in the
room or in me. Was there an insect, disappearing into the narrow gap under the refrigerator door? I try not to come
closer; I am afraid that if I really see it I will kill it, and I do not want to kill anything tonight. She is probably
at some factory-style opening, one of those white concrete rooms where Warhol's ghosts play on a loop and Maria Beatty’s
slow, anachronistic frames stain the plaster — images that barely move while the light on them changes. I half-remember
that same roach crawling across the frozen frames of Andy’s Empire, a tiny moving error on top of a motionless tower.
Where was the woman who said she'd come. Where is my muse.


## Setup and Installation

### Requirements

* **C++23** compiler - required standard for building the project
* **cxxopts** - command-line argument parsing used by the CLI tools
* **GTest** - Google Test framework used for unit testing
* **lcov** - generates coverage summaries from the unit test run
* **doxygen** and **graphviz** - build the API reference documentation and diagrams

### Building the Application

## Documentation and Contributing

To build and run tests, enable debug mode, or generate coverage reports:

1. **Build with Debug and Coverage:**
   ```bash
   $ cmake -B build CMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON -DCOVERAGE=ON
   ```
2. **Generate Coverage Report and HTML:**
   ```bash
   $ cmake --build build --target coverage
   ```

For detailed documentation, see the [Documentation](https://ninjaro.github.io/yodau/) and for the latest
coverage report, see [Coverage](https://ninjaro.github.io/yodau/cov/).

## Security Policy

Please report any security issues using GitHub's private vulnerability reporting
or by emailing [yaroslav.riabtsev@rwth-aachen.de](mailto:yaroslav.riabtsev@rwth-aachen.de).
See the [security policy](.github/SECURITY.md) for full details.

## License

This project is open-source and available under the MIT License.