// Feeds bytes to Cinder's Radiance (.hdr) image source and decodes it into a Surface32f.
#include "common.h"
#include "cinder/ImageSourceFileRadiance.h"
#include "cinder/Surface.h"
#include "cinder/Exception.h"

extern "C" int LLVMFuzzerTestOneInput( const uint8_t *data, size_t size )
{
	try {
		ci::Surface32f surface( ci::ImageSourceFileRadiance::create( fuzzDataSource( data, size ) ) );
	}
	catch( const ci::Exception & ) {
		return -1;
	}
	return 0;
}
