#include "util.h"
#include "fs.h"

std::string util::FindIncludedFile(const std::string& name, const std::string& filedir, const std::string& libdir)
{
	fs::path fpName{ConvertToNativeString(name)};
	fs::path fpFiledir{ConvertToNativeString(filedir)};
	fs::path fpLibdir{ConvertToNativeString(libdir)};
	// Complete paths aren't looked for in include directories
	if (fpName.is_absolute()) {
		return fs::exists( fpName )? name : std::string();
	}

	// First, try in the provided file directory.
	auto base = fpFiledir / fpName;
	if( fs::exists( base ) ) {
		return base.string();
	}

	// Next, try in the compilation working directory
	if( fs::exists( fpName ) ) {
		return name;
	}

	// Finally, check the libs directory, if it has been provided.
	if(!libdir.empty()) {
		auto libpath = fpLibdir / fpName;
		if( fs::exists(libpath) ) {
			return libpath.string();
		}
	}
	return std::string();
}

