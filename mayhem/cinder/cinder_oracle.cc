// Known-answer oracle for Cinder's own parsers: fixed input -> asserted value, through the public API.
#include "common.h"
#include "cinder/ImageSourceFileQoi.h"
#include "cinder/ImageSourceFileRadiance.h"
#include "cinder/JsonTree.h"
#include "cinder/ObjLoader.h"
#include "cinder/Surface.h"
#include "cinder/TriMesh.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

static int g_fail;

static void check( const char *name, bool ok, const std::string &detail = "" )
{
	if( ok ) printf( "PASS %s\n", name );
	else { printf( "FAIL %s %s\n", name, detail.c_str() ); g_fail++; }
}

static ci::DataSourceRef ds( const std::string &s )
{
	return fuzzDataSource( reinterpret_cast<const uint8_t *>( s.data() ), s.size() );
}

static ci::DataSourceRef ds( const std::vector<uint8_t> &v )
{
	return fuzzDataSource( v.data(), v.size() );
}

static void be32( std::vector<uint8_t> &v, uint32_t x )
{
	for( int s = 24; s >= 0; s -= 8 ) v.push_back( ( x >> s ) & 0xff );
}

int main()
{
	// ObjLoader: one quad with normals -> 4 unique vertices, 2 triangles (6 indices), positions kept.
	{
		ci::ObjLoader loader( ds( "v 0 0 0\nv 2 0 0\nv 2 3 0\nv 0 3 0\nvn 0 0 1\n"
		                          "f 1//1 2//1 3//1 4//1\n" ) );
		ci::TriMesh mesh( loader );
		check( "obj_vertex_count", mesh.getNumVertices() == 4, std::to_string( mesh.getNumVertices() ) );
		check( "obj_triangle_count", mesh.getNumTriangles() == 2, std::to_string( mesh.getNumTriangles() ) );
		ci::vec3 p = mesh.getPositions<3>()[2];
		check( "obj_position", p.x == 2.0f && p.y == 3.0f && p.z == 0.0f );
		check( "obj_normal", mesh.getNormals().size() == 4 && mesh.getNormals()[0].z == 1.0f );
	}
	// JsonTree: nested lookup, negative number, string, array size, serialization round trip.
	{
		ci::JsonTree t( "{\"a\":{\"b\":[10,20,30]},\"s\":\"hi\",\"n\":-42,\"f\":2.5}" );
		check( "json_nested_int", t.getChild( "a.b[1]" ).getValue<int>() == 20 );
		check( "json_array_size", t.getChild( "a.b" ).getNumChildren() == 3 );
		check( "json_string", t.getChild( "s" ).getValue() == "hi" );
		check( "json_negative", t.getChild( "n" ).getValue<int>() == -42 );
		check( "json_double", t.getChild( "f" ).getValue<double>() == 2.5 );
		ci::JsonTree again( t.serialize() );
		check( "json_roundtrip", again.getChild( "a.b[2]" ).getValue<int>() == 30 );
	}
	// Radiance: 2x1 flat RGBE; mantissa 128 with exponent byte 129 -> 128 * 2^(129-136) = 1.0.
	{
		std::string hdr = "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 1 +X 2\n";
		std::vector<uint8_t> v( hdr.begin(), hdr.end() );
		// The reader treats "stream at EOF after the last pixel" as truncation and leaves the last scanline
		// unconverted, so the data carries one trailing byte.
		const uint8_t px[9] = { 128, 64, 32, 129, 0, 0, 0, 0, 0 };
		v.insert( v.end(), px, px + 9 );
		ci::Surface32f s( ci::ImageSourceFileRadiance::create( ds( v ) ) );
		check( "radiance_size", s.getWidth() == 2 && s.getHeight() == 1 );
		ci::ColorAT<float> c = s.getPixel( ci::ivec2( 0, 0 ) );
		check( "radiance_pixel", std::fabs( c.r - 1.0f ) < 1e-3f && std::fabs( c.g - 0.5f ) < 1e-3f && std::fabs( c.b - 0.25f ) < 1e-3f,
		       std::to_string( c.r ) + "," + std::to_string( c.g ) + "," + std::to_string( c.b ) );
	}
	// QOI: 2x1 RGB; two QOI_OP_RGB pixels.
	{
		std::vector<uint8_t> v = { 'q', 'o', 'i', 'f' };
		be32( v, 2 ); be32( v, 1 );
		v.push_back( 3 ); v.push_back( 0 );
		const uint8_t body[] = { 0xFE, 10, 20, 30, 0xFE, 200, 100, 50, 0, 0, 0, 0, 0, 0, 0, 1 };
		v.insert( v.end(), body, body + sizeof body );
		ci::Surface8u s( ci::ImageSourceFileQoi::create( ds( v ), ci::ImageSource::Options() ) );
		check( "qoi_size", s.getWidth() == 2 && s.getHeight() == 1 );
		ci::ColorA8u a = s.getPixel( ci::ivec2( 0, 0 ) ), b = s.getPixel( ci::ivec2( 1, 0 ) );
		check( "qoi_pixel0", a.r == 10 && a.g == 20 && a.b == 30 );
		check( "qoi_pixel1", b.r == 200 && b.g == 100 && b.b == 50 );
	}
	// Malformed input is rejected with an exception, not accepted.
	{
		bool threw = false;
		try { ci::Surface8u s( ci::ImageSourceFileQoi::create( ds( "not a qoi image" ), ci::ImageSource::Options() ) ); }
		catch( const ci::Exception & ) { threw = true; }
		check( "qoi_rejects_garbage", threw );
		threw = false;
		try { ci::JsonTree t( "{\"a\": [1, 2" ); }
		catch( const ci::Exception & ) { threw = true; }
		check( "json_rejects_truncated", threw );
	}
	printf( g_fail ? "FAILED\n" : "DONE\n" );
	return g_fail ? 1 : 0;
}
