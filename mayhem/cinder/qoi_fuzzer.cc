// Feeds bytes to Cinder's QOI image source and decodes it into a Surface8u.
#include "common.h"
#include "cinder/ImageSourceFileQoi.h"
#include "cinder/Surface.h"
#include "cinder/Exception.h"

extern "C" int LLVMFuzzerTestOneInput( const uint8_t *data, size_t size )
{
	// Cap the declared image size (header bytes 4..11, big-endian): qoi.h accepts up to 400M pixels, so a
	// 14-byte input would otherwise allocate gigabytes and the run measures malloc, not the decoder.
	if( size >= 12 ) {
		uint64_t w = ( uint32_t( data[4] ) << 24 ) | ( data[5] << 16 ) | ( data[6] << 8 ) | data[7];
		uint64_t h = ( uint32_t( data[8] ) << 24 ) | ( data[9] << 16 ) | ( data[10] << 8 ) | data[11];
		if( w * h > ( 1u << 22 ) )
			return -1;
	}
	try {
		ci::Surface8u surface( ci::ImageSourceFileQoi::create( fuzzDataSource( data, size ), ci::ImageSource::Options() ) );
	}
	catch( const ci::Exception & ) {
		return -1;
	}
	return 0;
}
