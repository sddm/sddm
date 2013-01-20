pkgname=sddm-git
pkgver=20130120
pkgrel=1
pkgdesc="QML based X11 display manager" 
arch=('i686' 'x86_64')
url="http://github.com/sddm/sddm"
license=('GPL')
depends=('pam' 'qt')
makedepends=('cmake' 'git')
replaces=('sddm')

_gitroot="git://github.com/sddm/sddm.git"
_gitname=sddm

build() {
  cd "$srcdir"
  msg "Connecting to GIT server...."

  if [[ -d "$_gitname" ]]; then
    cd "$_gitname" && git pull origin
    msg "The local files are updated."
  else
    git clone "$_gitroot" "$_gitname"
  fi

  msg "GIT checkout done or server timeout"
  msg "Starting build..."

  mkdir -p build
  cd build

  cmake ../${_gitname} -DCMAKE_INSTALL_PREFIX=/usr
  make
}

package() {
  mkdir -p $pkgdir/usr/bin
  cp $srcdir/build/sddm $pkgdir/usr/bin

  mkdir -p $pkgdir/etc/pam.d
  cp $srcdir/$_gitname/sddm.conf $pkgdir/etc
  cp $srcdir/$_gitname/sddm.pam $pkgdir/etc/pam.d/sddm
  
  mkdir -p $pkgdir/usr/lib/systemd/system/
  cp $srcdir/$_gitname/sddm.service $pkgdir/usr/lib/systemd/system/

  mkdir -p $pkgdir/usr/share/config/sddm
  cp $srcdir/$_gitname/Xsession $pkgdir/usr/share/config/sddm
  chmod +x $pkgdir/usr/share/config/sddm/Xsession

  mkdir -p $pkgdir/usr/share/apps/sddm/themes
  cp -r $srcdir/$_gitname/themes/* $pkgdir/usr/share/apps/sddm/themes
  
  rm -rf $srcdir/build
}
