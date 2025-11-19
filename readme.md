## Year of the Depend Adult Undergarment<sup><a href="#ref-0">[0]</a></sup>

[![codecov](https://codecov.io/gh/ninjaro/yodau/graph/badge.svg?token=HDR73FWZU9)](https://codecov.io/gh/ninjaro/yodau)
[![Codacy Badge](https://app.codacy.com/project/badge/Grade/d89ee6b07aaa43f39f2561ee9eab5e89)](https://app.codacy.com/gh/ninjaro/yodau/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade)
[![license](https://img.shields.io/github/license/ninjaro/yodau?color=e6e6e6)](https://github.com/ninjaro/yodau/blob/master/license)  
[![Checks](https://github.com/ninjaro/yodau/actions/workflows/tests.yml/badge.svg)](https://github.com/ninjaro/yodau/actions/workflows/tests.yml)
[![Deploy](https://github.com/ninjaro/yodau/actions/workflows/html.yml/badge.svg)](https://github.com/ninjaro/yodau/actions/workflows/html.yml)
[![version](https://img.shields.io/github/v/release/ninjaro/yodau?include_prereleases)](https://github.com/ninjaro/yodau/releases/latest)

**Abstract.** She said she would come. I thought she'd come by nine. I pulled the Venetian blinds on the one window
whose yellow light kept spilling across the floor and making it harder to just sit and wait. I could not quite focus:
even without headsets, Lou Reed kept coming in waves, sometimes murmuring that he is waiting for a
man<sup><a href="#ref-1">[1]</a></sup>, sometimes hitting my right ear so hard my whole body twisted up; I pressed my
palm against the ringing eardrum, unsure whether it was in the room or in me. Was there an insect, disappearing into the
narrow gap under the refrigerator door? I try not to come closer; I am afraid that if I really see it I will kill it,
and I do not want to kill anything tonight. She is probably at some factory-style opening, one of those white concrete
rooms where Warhol's ghosts play on a loop and Maria Beatty’s slow, anachronistic frames stain the plaster — images that
barely move while the light on them changes<sup><a href="#ref-3">[3]</a></sup>. I half-remember that same roach crawling
across the frozen frames of Andy’s *Empire*<sup><a href="#ref-2">[2]</a></sup>. Where was the woman who said she'd
come.<sup><a href="#ref-0">[0]</a></sup> Where is my muse.

## Setup and Installation

### Requirements

The implementation and experiments in this work have been carried out with the following toolchain. Functionally
equivalent alternatives will usually work, although no strict lower bounds on versions are guaranteed.

* **C++23** compiler — a conforming implementation of ISO/IEC 14882:2024(E) (“C++23”), providing the standard library
  facilities used by the project.<sup><a href="#ref-4">[4]</a></sup>

* **cxxopts** — a lightweight C++ command-line option parser used for configuration of the CLI tools; any reasonably
  recent release of the library should be sufficient.<sup><a href="#ref-5">[5]</a></sup>

* **GoogleTest (GTest)** — Google’s C++ testing and mocking framework, used for the unit test suite and basic regression
  checks.<sup><a href="#ref-6">[6]</a></sup>

* **lcov** — tooling to collect and visualise line-coverage information from the test runs, typically in conjunction
  with `gcov` or similar backends.<sup><a href="#ref-7">[7]</a></sup>

* **doxygen** and **graphviz** — documentation and graph-visualisation tools used together to generate the API reference
  and structural diagrams from the annotated source code.<sup><a href="#ref-8">[8]</a>, <a href="#ref-9">[9]</a></sup>

Exact version numbers are intentionally left vague; the project currently builds with mainstream C++23 toolchains and
commonly packaged versions of the above utilities.

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

## Not-DFW Footnotes

<a id="ref-0"></a>[0] D. F. Wallace, *Infinite Jest*, 1st ed. Boston, MA, USA: Little, Brown and Company, 1996, pp.
17–27.

<a id="ref-1"></a>[1] L. Reed, “I’m Waiting for the Man,” in *The Velvet Underground & Nico* [LP]. New York, NY, USA:
Verve Records, 1967, track 2.

<a id="ref-2"></a>[2] A. Warhol and J. Palmer, *Empire* [Film]. USA: Andy Warhol Films, 1964, 485 min., black-and-white,
silent.

<a id="ref-3"></a>[3] M. Beatty, *Ecstasy in Berlin, 1926* [Video]. USA: Bleu Productions, 2004, 45 min.,
black-and-white and tinted, silent with music; DVD release, 2005.

<a id="ref-4"></a>[4] ISO/IEC, *ISO/IEC 14882:2024 — Programming Languages — C++*. Geneva, Switzerland:
International Organization for Standardization, 2024.

<a id="ref-5"></a>[5] J. Arroyo, *cxxopts* [Software]. GitHub repository, “jarro2783/cxxopts”, accessed Nov. 2025.
[Online]. Available: [https://github.com/jarro2783/cxxopts](https://github.com/jarro2783/cxxopts)

<a id="ref-6"></a>[6] Google, *GoogleTest: Google Testing and Mocking Framework* [Software]. GitHub repository,
“google/googletest”, accessed Nov. 2025. [Online].
Available: [https://github.com/google/googletest](https://github.com/google/googletest)

<a id="ref-7"></a>[7] Linux Test Project, *LCOV* [Software]. GitHub repository, “linux-test-project/lcov”, accessed
Nov. 2025. [Online]. Available: [https://github.com/linux-test-project/lcov](https://github.com/linux-test-project/lcov)

<a id="ref-8"></a>[8] D. van Heesch, *Doxygen* [Software]. Doxygen project website, accessed Nov. 2025.
[Online]. Available: [https://www.doxygen.nl/](https://www.doxygen.nl/)

<a id="ref-9"></a>[9] J. Ellson, E. Gansner, E. Koutsofios, S. C. North, and G. Woodhull, “Graphviz—Open Source
Graph Drawing Tools,” in *Graph Drawing (GD 2001)*, P. Mutzel, M. Jünger, and S. Leipert, Eds., Lecture Notes in
Computer Science, vol. 2265. Berlin, Germany: Springer, 2002, pp. 483–484.
