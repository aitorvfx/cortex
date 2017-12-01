//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2009-2010, Image Engine Design Inc. All rights reserved.
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

// This include needs to be the very first to prevent problems with warnings
// regarding redefinition of _POSIX_C_SOURCE
#include "boost/python.hpp"

#include "IECore/CompoundObject.h"
#include "IECore/CompoundParameter.h"
#include "IECore/MessageHandler.h"
#include "IECoreScene/Renderer.h"
#include "IECoreScene/ParameterisedProcedural.h"
#include "IECorePython/RunTimeTypedBinding.h"
#include "IECorePython/ScopedGILLock.h"
#include "IECorePython/ScopedGILRelease.h"

#include "ParameterisedProceduralBinding.h"

using namespace boost::python;
using namespace IECore;
using namespace IECorePython;
using namespace IECoreScene;
using namespace IECoreSceneModule;

namespace
{

class ParameterisedProceduralWrapper : public RunTimeTypedWrapper<ParameterisedProcedural>
{

	public :

		ParameterisedProceduralWrapper( PyObject *self, const std::string &description="" )
			: RunTimeTypedWrapper<ParameterisedProcedural>( self, description )
		{
		}

		void doRenderState( RendererPtr renderer, ConstCompoundObjectPtr args ) const override
		{
			if( isSubclassed() )
			{
				ScopedGILLock gilLock;
				try
				{
					object o = this->methodOverride( "doRenderState" );
					if( o )
					{
						o( renderer, boost::const_pointer_cast<CompoundObject>( args ) );
						return;
					}
					else
					{
						ParameterisedProcedural::doRenderState( renderer, args );
					}
				}
				catch( error_already_set )
				{
					PyErr_Print();
				}
				catch( const std::exception &e )
				{
					msg( Msg::Error, "ParameterisedProceduralWrapper::doRenderState", e.what() );
				}
				catch( ... )
				{
					msg( Msg::Error, "ParameterisedProceduralWrapper::doRenderState", "Caught unknown exception" );
				}
			}

			ParameterisedProcedural::doRenderState( renderer, args );
		}

		Imath::Box3f doBound( ConstCompoundObjectPtr args ) const override
		{
			ScopedGILLock gilLock;
			try
			{
				object o = this->methodOverride( "doBound" );
				if( o )
				{
					return extract<Imath::Box3f>( o( boost::const_pointer_cast<CompoundObject>( args ) ) );
				}
				else
				{
					msg( Msg::Error, "ParameterisedProceduralWrapper::doBound", "doBound() python method not defined" );
				}
			}
			catch( error_already_set )
			{
				PyErr_Print();
			}
			catch( const std::exception &e )
			{
				msg( Msg::Error, "ParameterisedProceduralWrapper::doBound", e.what() );
			}
			catch( ... )
			{
				msg( Msg::Error, "ParameterisedProceduralWrapper::doBound", "Caught unknown exception" );
			}

			return Imath::Box3f(); // empty
		}

		void doRender( RendererPtr r, ConstCompoundObjectPtr args ) const override
		{
			ScopedGILLock gilLock;
			// ideally we might not do any exception handling here, and always leave it to the host.
			// but in our case the host is mainly 3delight and that does no exception handling at all.
			try
			{
				object o = this->methodOverride( "doRender" );
				if( o )
				{
					o( r, boost::const_pointer_cast<CompoundObject>( args ) );
					return;
				}
				else
				{
					msg( Msg::Error, "ParameterisedProceduralWrapper::doRender", "doRender() python method not defined" );
				}
			}
			catch( error_already_set )
			{
				PyErr_Print();
			}
			catch( const std::exception &e )
			{
				msg( Msg::Error, "ParameterisedProceduralWrapper::doRender", e.what() );
			}
			catch( ... )
			{
				msg( Msg::Error, "ParameterisedProceduralWrapper::doRender", "Caught unknown exception" );
			}
		}

};

static void render( ParameterisedProcedural &o, Renderer *renderer )
{
	ScopedGILRelease gilRelease;
	o.render( renderer );
}

static void render2( ParameterisedProcedural &o, Renderer *renderer, bool inAttributeBlock, bool withState, bool withGeometry, bool immediateGeometry )
{
	ScopedGILRelease gilRelease;
	o.render( renderer, inAttributeBlock, withState, withGeometry, immediateGeometry );
}

static ParameterPtr parameterisedProceduralGetItem( ParameterisedProcedural &o, const std::string &n )
{
	ParameterPtr p = o.parameters()->parameter<Parameter>( n );
	if( !p )
	{
		throw Exception( std::string("Parameter ") + n + " doesn't exist" );
	}
	return p;
}

} // namespace

namespace IECoreSceneModule
{

void bindParameterisedProcedural()
{

	CompoundParameter *(ParameterisedProcedural::*parameters)() = &ParameterisedProcedural::parameters;

	RunTimeTypedClass<ParameterisedProcedural, ParameterisedProceduralWrapper>()
		.def( init<>() )
		.def( init< const std::string >( arg( "description") ) )
		.add_property( "description", make_function( &ParameterisedProcedural::description, return_value_policy<copy_const_reference>() ) )
		.def( "parameters", parameters, return_value_policy<CastToIntrusivePtr>() )
		.def( "render", &render )
		.def( "render", &render2, ( arg( "renderer" ), arg( "inAttributeBlock" ) = true, arg( "withState" ) = true, arg( "withGeometry" ) = true, arg( "immediateGeometry" ) = false ) )
		.def( "__getitem__", &parameterisedProceduralGetItem )
	;

}

} // namespace IECoreSceneModule