// A GUI-free definition of cinder::app::PlatformLinux for the Cinder-API fuzz targets.
// The real src/cinder/app/linux/PlatformLinux.cpp links the whole app/window/GL/GLFW/audio layer
// (AppBase, Window, Display, FontManager, ...), none of which the parsers fuzzed here touch; they
// only reach Platform through Platform::get() (asset paths, console). Registers the same image
// source types the real constructor does for the formats under test.
#include "cinder/app/linux/PlatformLinux.h"
#include "cinder/ImageSourceFileQoi.h"
#include "cinder/ImageSourceFileRadiance.h"
#include "cinder/Url.h"

#include <cstdlib>
#include <unistd.h>

namespace cinder { namespace app {

PlatformLinux::PlatformLinux()
{
	ImageSourceFileRadiance::registerSelf();
	ImageSourceFileQoi::registerSelf();
}

PlatformLinux::~PlatformLinux() {}

PlatformLinux* PlatformLinux::get() { return reinterpret_cast<PlatformLinux*>( Platform::get() ); }

void PlatformLinux::cleanupLaunch() {}

DataSourceRef PlatformLinux::loadResource( const fs::path &resourcePath )
{
	return DataSourcePath::create( resourcePath );
}

fs::path PlatformLinux::getResourceDirectory() const { return fs::path(); }
fs::path PlatformLinux::getResourcePath( const fs::path &rsrcRelativePath ) const { return rsrcRelativePath; }

fs::path PlatformLinux::getOpenFilePath( const fs::path &, const std::vector<std::string> & ) { return fs::path(); }
fs::path PlatformLinux::getFolderPath( const fs::path & ) { return fs::path(); }
fs::path PlatformLinux::getSaveFilePath( const fs::path &, const std::vector<std::string> & ) { return fs::path(); }

std::map<std::string, std::string> PlatformLinux::getEnvironmentVariables() { return {}; }

fs::path PlatformLinux::expandPath( const fs::path &path ) { return path; }

fs::path PlatformLinux::getHomeDirectory() const
{
	const char *home = getenv( "HOME" );
	return fs::path( home ? home : "/" );
}

fs::path PlatformLinux::getDocumentsDirectory() const { return getHomeDirectory(); }
fs::path PlatformLinux::getDefaultExecutablePath() const { return fs::path( "/proc/self/exe" ); }

void PlatformLinux::sleep( float ) {}
void PlatformLinux::launchWebBrowser( const Url & ) {}
std::vector<std::string> PlatformLinux::stackTrace() { return {}; }
void PlatformLinux::setThreadName( const std::string & ) {}

const std::vector<DisplayRef>& PlatformLinux::getDisplays() { return mDisplays; }

}} // namespace cinder::app
