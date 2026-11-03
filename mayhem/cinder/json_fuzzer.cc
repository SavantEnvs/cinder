// Feeds a string to ci::JsonTree (JSON parser + tree API) and re-serializes it.
#include "cinder/JsonTree.h"
#include "cinder/Exception.h"
#include <string>

extern "C" int LLVMFuzzerTestOneInput( const uint8_t *data, size_t size )
{
	try {
		ci::JsonTree tree( std::string( reinterpret_cast<const char *>( data ), size ) );
		(void)tree.serialize();
		for( const ci::JsonTree &child : tree.getChildren() )
			(void)child.getKey();
	}
	catch( const ci::Exception & ) {
		return -1;
	}
	return 0;
}
