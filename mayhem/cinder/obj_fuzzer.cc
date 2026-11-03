// Feeds bytes to ci::ObjLoader (Wavefront OBJ parser) and builds a TriMesh from it.
#include "common.h"
#include "cinder/ObjLoader.h"
#include "cinder/TriMesh.h"
#include "cinder/Exception.h"

extern "C" int LLVMFuzzerTestOneInput( const uint8_t *data, size_t size )
{
	try {
		ci::ObjLoader loader( fuzzDataSource( data, size ) );
		ci::TriMesh mesh( loader );
		(void)mesh.getNumVertices();
	}
	catch( const ci::Exception & ) {
		return -1;
	}
	return 0;
}
