//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2010-2014, Image Engine Design Inc. All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are
//  met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//
//     * Neither the name of Image Engine Design nor the names of any
//       other contributors to this software may be used to endorse or
//       promote products derived from this software without specific prior
//       written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
//  IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
//  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
//  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
//  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
//  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
//  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
//  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
//  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
//  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
//  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////

#include "IECoreHoudini/ToHoudiniGeometryConverter.h"

#include "IECoreHoudini/Convert.h"
#include "IECoreHoudini/ToHoudiniAttribConverter.h"
#include "IECoreHoudini/ToHoudiniStringAttribConverter.h"

#include "IECore/CompoundData.h"
#include "IECore/CompoundParameter.h"
#include "IECore/MessageHandler.h"

#include "SYS/SYS_Types.h"
#include "UT/UT_StringMMPattern.h"

using namespace IECore;
using namespace IECoreScene;
using namespace IECoreHoudini;

IE_CORE_DEFINERUNTIMETYPED( ToHoudiniGeometryConverter );

ToHoudiniGeometryConverter::ToHoudiniGeometryConverter( const IECore::Object *object, const std::string &description )
	:	ToHoudiniConverter( description, ObjectTypeId )
{
	srcParameter()->setValue( const_cast<Object*>( object ) ); // safe because the object is const in doConversion

	m_nameParameter = new StringParameter(
		"name",
		"The name given to the converted primitive(s). If empty, primitives will be unnamed",
		""
	);

	m_attributeFilterParameter = new StringParameter(
		"attributeFilter",
		"A list of attribute names to convert, if they exist. Uses Houdini matching syntax.",
		"*"
	);

	m_convertStandardAttributesParameter = new BoolParameter(
		"convertStandardAttributes",
		"Performs automated conversion of standard PrimitiveVariables to Houdini Attributes (i.e. Pref->rest ; Cs->Cd)",
		true
	);

	parameters()->addParameter( m_nameParameter );
	parameters()->addParameter( m_attributeFilterParameter );
	parameters()->addParameter( m_convertStandardAttributesParameter );
}

ToHoudiniGeometryConverter::~ToHoudiniGeometryConverter()
{
}

BoolParameter *ToHoudiniGeometryConverter::convertStandardAttributesParameter()
{
	return m_convertStandardAttributesParameter.get();
}

const BoolParameter *ToHoudiniGeometryConverter::convertStandardAttributesParameter() const
{
	return m_convertStandardAttributesParameter.get();
}

StringParameter *ToHoudiniGeometryConverter::nameParameter()
{
	return m_nameParameter.get();
}

const StringParameter *ToHoudiniGeometryConverter::nameParameter() const
{
	return m_nameParameter.get();
}

StringParameter *ToHoudiniGeometryConverter::attributeFilterParameter()
{
	return m_attributeFilterParameter.get();
}

const StringParameter *ToHoudiniGeometryConverter::attributeFilterParameter() const
{
	return m_attributeFilterParameter.get();
}

bool ToHoudiniGeometryConverter::convert( GU_DetailHandle handle ) const
{
	ConstCompoundObjectPtr operands = parameters()->getTypedValidatedValue<CompoundObject>();
	GU_DetailHandleAutoWriteLock writeHandle( handle );

	GU_Detail *geo = writeHandle.getGdp();
	if ( !geo )
	{
		return false;
	}

	bool result = doConversion( srcParameter()->getValidatedValue(), geo );
	if ( result )
	{
		geo->incrementMetaCacheCount();
	}

	return result;
}

GA_Range ToHoudiniGeometryConverter::appendPoints( GA_Detail *geo, size_t numPoints ) const
{
	if ( !numPoints )
	{
		return GA_Range();
	}

	GA_Offset firstPoint = geo->appendPointBlock( numPoints );

	return GA_Range( geo->getPointMap(), firstPoint, firstPoint + numPoints );
}

PrimitiveVariable ToHoudiniGeometryConverter::processPrimitiveVariable( const IECoreScene::Primitive *primitive, const PrimitiveVariable &primVar ) const
{
	return primVar;
}

void ToHoudiniGeometryConverter::transferAttribs( GU_Detail *geo, const GA_Range &points, const GA_Range &prims ) const
{
	const Primitive *primitive = IECore::runTimeCast<const Primitive>( srcParameter()->getValidatedValue() );
	if ( primitive )
	{
		transferAttribValues( primitive, geo, points, prims );
	}

	setName( geo, prims );
}

void ToHoudiniGeometryConverter::setName( GU_Detail *geo, const GA_Range &prims ) const
{
	// add the name attribute based on the parameter
	const std::string &name = nameParameter()->getTypedValue();
	if ( name != "" && prims.isValid() )
	{
		ToHoudiniStringVectorAttribConverter::convertString( "name", name, geo, prims );
	}
}

void ToHoudiniGeometryConverter::transferAttribValues(
	const Primitive *primitive, GU_Detail *geo,
	const GA_Range &points, const GA_Range &prims,
	PrimitiveVariable::Interpolation vertexInterpolation,
	PrimitiveVariable::Interpolation primitiveInterpolation,
	PrimitiveVariable::Interpolation pointInterpolation,
	PrimitiveVariable::Interpolation detailInterpolation
) const
{
	GA_OffsetList offsets;
	if( prims.isValid() )
	{
		const GA_PrimitiveList &primitives = geo->getPrimitiveList();

		GA_Offset start, end;
		for( GA_Iterator it( prims ); it.blockAdvance( start, end ); )
		{
			for( GA_Offset offset = start; offset < end; ++offset )
			{
				const GA_Primitive *prim = primitives.get( offset );
				/// \todo: we shouldn't reverse winding for open polys (eg linear curves)
				bool reverseWinding = ( prim->getTypeId() == GEO_PRIMPOLY );
				size_t numPrimVerts = prim->getVertexCount();
				for( size_t v = 0; v < numPrimVerts; v++ )
				{
					if( reverseWinding )
					{
						offsets.append( prim->getVertexOffset( numPrimVerts - 1 - v ) );
					}
					else
					{
						offsets.append( prim->getVertexOffset( v ) );
					}
				}
			}
		}
	}

	GA_Range vertRange( geo->getVertexMap(), offsets );

	UT_String filter( attributeFilterParameter()->getTypedValue() );

	bool convertStandardAttributes = m_convertStandardAttributesParameter->getTypedValue();

	// process all primvars with UV interpretation
	for ( const auto &it : primitive->variables)
	{

		if ( !UT_String( it.first ).multiMatch( filter ) )
		{
			continue;
		}

		if (const V2fVectorData *uvData = runTimeCast<const V2fVectorData> ( it.second.data.get() ) )
		{
			if ( uvData->getInterpretation() != GeometricData::UV )
			{
				continue;
			}

			PrimitiveVariable::IndexedView<Imath::V2f> uvIndexedView ( it.second );

			// Houdini prefers a V3f uvw rather than V2f uv,
			// though they advise setting the 3rd component to 0.
			std::vector<Imath::V3f> uvw;
			uvw.reserve( uvIndexedView.size() );
			for ( size_t i=0; i < uvIndexedView.size(); ++i )
			{
				uvw.emplace_back( uvIndexedView[i][0], uvIndexedView[i][1], 0 );
			}

			GA_Range range;
			if( it.second.interpolation == pointInterpolation )
			{
				range = points;
			}
			else if( it.second.interpolation == primitiveInterpolation )
			{
				range = prims;
			}
			else if( it.second.interpolation == vertexInterpolation )
			{
				range = vertRange;
			}
			else
			{
				IECore::msg(
					IECore::MessageHandler::Warning,
					"ToHoudiniGeometryConverter",
					"UV PrimitiveVariable '" + it.first + "' has invalid interpolation. Ignoring."
				);
				continue;
			}
			

			V3fVectorData::Ptr uvwData = new V3fVectorData( uvw );
			uvwData->setInterpretation( GeometricData::UV );

			ToHoudiniAttribConverterPtr converter = ToHoudiniAttribConverter::create( uvwData.get() );
			converter->convert( it.first, geo, range );
			filter += " ^" + it.first;
		}
	}


 	UT_StringMMPattern attribFilter;
	attribFilter.compile( filter );

	// add the primitive variables to the various GEO_AttribDicts based on interpolation type
	for ( PrimitiveVariableMap::const_iterator it=primitive->variables.begin() ; it != primitive->variables.end(); it++ )
	{
		if( !primitive->isPrimitiveVariableValid( it->second ) )
		{
			IECore::msg( IECore::MessageHandler::Warning, "ToHoudiniGeometryConverter", "PrimitiveVariable " + it->first + " is invalid. Ignoring." );
			continue;
		}

		UT_String varName( it->first );
		if ( !varName.multiMatch( attribFilter ) )
		{
			continue;
		}

		PrimitiveVariable primVar = processPrimitiveVariable( primitive, it->second );

		DataPtr data = nullptr;
		ToHoudiniAttribConverterPtr converter = nullptr;

		if( primVar.indices && primVar.data->typeId() == StringVectorDataTypeId )
		{
			// we want to process the indexed strings rather than the expanded strings
			converter = ToHoudiniAttribConverter::create( primVar.data.get() );
			if( ToHoudiniStringVectorAttribConverter *stringVectorConverter = IECore::runTimeCast<ToHoudiniStringVectorAttribConverter>( converter.get() ) )
			{
				stringVectorConverter->indicesParameter()->setValidatedValue( primVar.indices.get() );
			}
		}
		else
		{
			// all other primitive variables must be expanded
			data = primVar.expandedData();
			converter = ToHoudiniAttribConverter::create( data.get() );
		}

		if ( !converter )
		{
			continue;
		}

		const std::string name = ( convertStandardAttributes ) ? processPrimitiveVariableName( it->first ) : it->first;

		if ( primVar.interpolation == detailInterpolation )
 		{
			// add detail attribs
			try
			{
				converter->convert( name, geo );
			}
			catch ( std::exception &e )
			{
				throw IECore::Exception( "PrimitiveVariable \"" + it->first + "\" could not be converted as a Detail Attrib: " + e.what() );
			}
	 	}
		else if ( primVar.interpolation == pointInterpolation )
		{

#if UT_MAJOR_VERSION_INT < 15

			// add point attribs
			if ( name == "P" )
			{
				// special case for P
				transferP( runTimeCast<const V3fVectorData>( primVar.data.get() ), geo, points );
			}
			else

#endif

			{
 				try
				{
					GA_RWAttributeRef attrRef = converter->convert( name, geo, points );

					// mark rest as non-transforming so it doesn't get manipulated once inside Houdini
					if ( name == "rest" || name == "Pref" )
					{

#if UT_MAJOR_VERSION_INT >= 15

						attrRef.setTypeInfo( GA_TYPE_VOID );

#else

						attrRef.getAttribute()->setNonTransforming( true );

#endif

					}
				}
				catch ( std::exception &e )
				{
					throw IECore::Exception( "PrimitiveVariable \"" + it->first + "\" could not be converted as a Point Attrib: " + e.what() );
				}
			}
		}
		else if ( primVar.interpolation == primitiveInterpolation )
		{
			// add primitive attribs
			try
			{
				converter->convert( name, geo, prims );
			}
			catch ( std::exception &e )
			{
				throw IECore::Exception( "PrimitiveVariable \"" + it->first + "\" could not be converted as a Primitive Attrib: " + e.what() );
			}
		}
		else if ( primVar.interpolation == vertexInterpolation )
		{
			// add vertex attribs
			try
			{
				converter->convert( name, geo, vertRange );
			}
			catch ( std::exception &e )
			{
				throw IECore::Exception( "PrimitiveVariable \"" + it->first + "\" could not be converted as a Vertex Attrib: " + e.what() );
			}
		}
	}

	// backwards compatibility with older data
	const StringData *nameData = primitive->blindData()->member<StringData>( "name" );
	if ( nameData && prims.isValid() )
	{
		ToHoudiniStringVectorAttribConverter::convertString( "name", nameData->readable(), geo, prims );
	}
}

void ToHoudiniGeometryConverter::transferP( const IECore::V3fVectorData *positions, GU_Detail *geo, const GA_Range &points ) const
{
	if ( !positions )
	{
		return;
	}

	const std::vector<Imath::V3f> &pos = positions->readable();

	size_t i = 0;
	for ( GA_Iterator it=points.begin(); !it.atEnd(); ++it, ++i )
	{
		geo->setPos3( it.getOffset(), IECore::convert<UT_Vector3>( pos[i] ) );
	}
}

const std::string ToHoudiniGeometryConverter::processPrimitiveVariableName( const std::string &name ) const
{
	/// \todo: This should probably be some formal static map. Make sure to update FromHoudiniGeometryConverter as well.
	if ( name == "Cs" )
	{
		return "Cd";
	}
	else if ( name == "Os" )
	{
		return "Alpha";
	}
	else if ( name == "Pref" )
	{
		return "rest";
	}
	else if ( name == "width" )
	{
		return "pscale";
	}

	return name;
}

/////////////////////////////////////////////////////////////////////////////////
// Factory
/////////////////////////////////////////////////////////////////////////////////

ToHoudiniGeometryConverterPtr ToHoudiniGeometryConverter::create( const Object *object )
{
	const TypesToFnsMap *m = typesToFns();
	TypesToFnsMap::const_iterator it = m->find( Types( object->typeId() ) );
	if( it!=m->end() )
	{
		return it->second( object );
	}

	// no exact match, so check for base class matches
	const std::vector<IECore::TypeId> &bases = RunTimeTyped::baseTypeIds( object->typeId() );
	for ( std::vector<IECore::TypeId>::const_iterator it = bases.begin(); it != bases.end(); ++it )
	{
		TypesToFnsMap::const_iterator cIt = m->find( Types( *it ) );
		if ( cIt != m->end() )
		{
			return cIt->second( object );
		}
	}

	return 0;
}

void ToHoudiniGeometryConverter::registerConverter( IECore::TypeId fromType, CreatorFn creator )
{
	TypesToFnsMap *m = typesToFns();
	m->insert( TypesToFnsMap::value_type( Types( fromType ), creator ) );
}

ToHoudiniGeometryConverter::TypesToFnsMap *ToHoudiniGeometryConverter::typesToFns()
{
	static TypesToFnsMap *m = new TypesToFnsMap;
	return m;
}

void ToHoudiniGeometryConverter::supportedTypes( std::set<IECore::TypeId> &types )
{
	types.clear();

	const TypesToFnsMap *m = typesToFns();
	for ( TypesToFnsMap::const_iterator it=m->begin(); it != m->end(); it ++ )
	{
		types.insert( it->first.fromType );
	}
}

/////////////////////////////////////////////////////////////////////////////////
// Implementation of nested Types class
/////////////////////////////////////////////////////////////////////////////////

ToHoudiniGeometryConverter::Types::Types( IECore::TypeId from ) : fromType( from )
{
}

bool ToHoudiniGeometryConverter::Types::operator < ( const Types &other ) const
{
	return fromType < other.fromType;
}
