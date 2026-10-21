cask "orbital" do
  version "0.3.2"
  sha256 :no_check

  url "https://github.com/kefy266/orbital/releases/download/v#{version}/Orbital-macos-arm64.dmg"
  name "Orbital"
  desc "Skill ve eklenti destekli, sade ve güçlü masaüstü AI kod asistanı"
  homepage "https://kefy266.github.io/orbital"

  depends_on macos: ">= :monterey"

  app "Orbital.app"

  postflight do
    system_command "xattr",
                   args: ["-cr", "#{appdir}/Orbital.app"],
                   sudo: false
  end

  zap trash: [
    "~/.config/orbital",
    "~/.local/share/orbital",
  ]
end
