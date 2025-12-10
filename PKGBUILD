pkgname=yodau
pkgver=0.1.0
pkgrel=1
pkgdesc="YEAR OF THE DEPEND ADULT UNDERGARMENT: Qt/OpenCV-based video stream viewer with motion and tripwire events"
arch=('x86_64')
url="https://github.com/ninjaro/yodau"
license=('MIT')

depends=(
  'qt6-base'
  'qt6-multimedia'
)
optdepends=(
  'opencv: enables motion/tripwire detection backend'
)
makedepends=(
  'cmake'
  'ninja'
  'gcc'
)

source=("$pkgname-$pkgver.tar.gz::$url/archive/refs/tags/v$pkgver.tar.gz")
sha256sums=('9067af7856c0a53274e493eab0dc56abe24e06393a2627542af70884163c265d')

build() {
  cmake -S "$srcdir/$pkgname-$pkgver" -B "$srcdir/build" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DKDE=OFF \
    -DBUILD_GUI=ON \
    -DBUILD_TESTS=OFF \
    -DENABLE_COVERAGE=OFF \
    -DENABLE_ASAN=OFF
  cmake --build "$srcdir/build"
}

package() {
  DESTDIR="$pkgdir" cmake --install "$srcdir/build"
}
