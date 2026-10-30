class Orbital < Formula
  desc "Skill ve eklenti destekli, sade ve güçlü masaüstü AI kod asistanı"
  homepage "https://kefy266.github.io/orbital"
  url "https://github.com/kefy266/orbital.git", tag: "v0.3.8"
  version "0.3.8"
  license "MIT"
  head "https://github.com/kefy266/orbital.git", branch: "main"

  depends_on "qt"

  def install
    system Formula["qt"].opt_bin/"qmake", "CONFIG+=release", "orbital.pro", "-o", "build.mk"
    system "make", "-f", "build.mk"

    if OS.mac?
      prefix.install "build/Orbital.app"
      bin.write_exec_script prefix/"Orbital.app/Contents/MacOS/Orbital"
    else
      bin.install "build/orbital"
    end
  end

  def caveats
    <<~EOS
      Orbital şuraya kuruldu:
        #{opt_prefix}/Orbital.app

      Terminalden doğrudan çalıştırmak için:
        orbital

      Uygulamalar (Applications) klasörüne eklemek için:
        ln -sf #{opt_prefix}/Orbital.app /Applications/Orbital.app
    EOS
  end

  test do
    assert_predicate prefix/"Orbital.app/Contents/MacOS/Orbital", :exist?
  end
end
