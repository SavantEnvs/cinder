// Shared by the Cinder-API harnesses: wrap fuzz bytes in a DataSourceBuffer, the way an app feeds
// in-memory data to Cinder's loaders.
#pragma once
#include "cinder/Buffer.h"
#include "cinder/DataSource.h"
#include <cstdint>
#include <cstring>

static inline ci::DataSourceRef fuzzDataSource( const uint8_t *data, size_t size )
{
	ci::BufferRef buf = ci::Buffer::create( size );
	if( size )
		memcpy( buf->getData(), data, size );
	return ci::DataSourceBuffer::create( buf );
}
