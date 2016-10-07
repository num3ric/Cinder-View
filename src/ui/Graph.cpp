/*
 Copyright (c) 2015, The Cinder Project, All rights reserved.

 This code is intended for use with the Cinder C++ library: http://libcinder.org

 Redistribution and use in source and binary forms, with or without modification, are permitted provided
 that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, this list of conditions and
 the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
 the following disclaimer in the documentation and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
*/

#include "ui/Graph.h"

#include "cinder/app/AppBase.h"
#include "cinder/Log.h"

//#define LOG_TOUCHES( stream )	CI_LOG_I( stream )
#define LOG_TOUCHES( stream )	( (void)( 0 ) )

using namespace ci;
using namespace std;

namespace ui {

Graph::Graph( const ci::app::WindowRef &window )
	: mWindow( window )
{
	mGraph = this;

	// The Graph always gets a Layer because it is root.
	mLayer = makeLayer( this );

	if( ! mWindow ) {
		auto app = app::AppBase::get();
		if( ! app ) {
			throw GraphExc( "Running app-less, must provide an app::Window" );
		}

		mWindow = app->getWindow();
		mMultiTouchEnabled = app->isMultiTouchEnabled();
		setBounds( mWindow->getBounds() );
	}

	mRenderer = make_shared<ui::Renderer>();
}

Graph::~Graph()
{
	if( ! mEventConnections.empty() )
		disconnectEvents();
}

void Graph::setNeedsLayer( View *view )
{
	makeLayer( view );
}

LayerRef Graph::makeLayer( View *rootView )
{
	auto result = make_shared<Layer>( rootView );
	result->mGraph = this;
	rootView->mLayer = result;
	mLayers.push_back( result );

	result->init();
	return result;
}

void Graph::removeLayer( const LayerRef &layer )
{
	layer->markForRemoval();
	layer->getRootView()->mLayer = nullptr;
}

void Graph::setClippingSize( const ci::ivec2 &size )
{
	mClippingSize = size;
	mClippingSizeSet = true;
}

//! Returns the size used for clipping operations in pixel coordinates. Defaults to the size of the window
ci::ivec2 Graph::getClippingSize() const
{
	return ( mClippingSizeSet ? mClippingSize : mWindow->getSize() );
}

void Graph::layout()
{
	if( isFillParentEnabled() ) {
		setSize( mWindow->getSize() );
	}
}

void Graph::propagateUpdate()
{
	mLayer->update();

	for( auto layerIt = mLayers.begin(); layerIt != mLayers.end(); /* */ ) {
		auto &layer = *layerIt;

		if( layer->getShouldRemove() ) {
			layerIt = mLayers.erase( layerIt );
			continue;
		}
		else {
			++layerIt;
		}
	}

	// clear any Views that were marked for removal
	mViewsWithTouches.erase(
			remove_if( mViewsWithTouches.begin(), mViewsWithTouches.end(),
			           []( const ViewRef &view ) {
				           return view->mMarkedForRemoval;
			           } ),
			mViewsWithTouches.end() );
}

void Graph::propagateDraw()
{
	CI_ASSERT( getLayer() );

	mLayer->draw( mRenderer.get() );
}

// ----------------------------------------------------------------------------------------------------
// Events
// ----------------------------------------------------------------------------------------------------

void Graph::connectEvents( const EventOptions &options )
{
	mEventSlotPriority = options.mPriority;

	if( ! mEventConnections.empty() )
		disconnectEvents();

	if( options.mMouse ) {
		mEventConnections.push_back( mWindow->getSignalMouseDown().connect( mEventSlotPriority, [&]( app::MouseEvent &event ) {
			app::TouchEvent touchEvent( event.getWindow(), vector<app::TouchEvent::Touch>( 1, app::TouchEvent::Touch( event.getPos(), vec2( 0 ), 0, 0, &event ) ) );
			propagateTouchesBegan( touchEvent );
			event.setHandled( touchEvent.isHandled() );
			mPrevMousePos = event.getPos();
		} ) );
		mEventConnections.push_back( mWindow->getSignalMouseDrag().connect( mEventSlotPriority, [&]( app::MouseEvent &event ) {
			app::TouchEvent touchEvent( event.getWindow(), vector<app::TouchEvent::Touch>( 1, app::TouchEvent::Touch( event.getPos(), mPrevMousePos, 0, 0, &event ) ) );
			propagateTouchesMoved( touchEvent );
			event.setHandled( touchEvent.isHandled() );
			mPrevMousePos = event.getPos();
		} ) );
		mEventConnections.push_back( mWindow->getSignalMouseUp().connect( mEventSlotPriority, [&]( app::MouseEvent &event ) {
			app::TouchEvent touchEvent( event.getWindow(), vector<app::TouchEvent::Touch>( 1, app::TouchEvent::Touch( event.getPos(), mPrevMousePos, 0, 0, &event ) ) );
			propagateTouchesEnded( touchEvent );
			event.setHandled( touchEvent.isHandled() );
			mPrevMousePos = event.getPos();
		} ) );
	}

	if( options.mTouches ) {
		mEventConnections.push_back( mWindow->getSignalTouchesBegan().connect( mEventSlotPriority, [&]( app::TouchEvent &event ) {
			propagateTouchesBegan( event );
		} ) );
		mEventConnections.push_back( mWindow->getSignalTouchesMoved().connect( mEventSlotPriority, [&]( app::TouchEvent &event ) {
			propagateTouchesMoved( event );
		} ) );
		mEventConnections.push_back( mWindow->getSignalTouchesEnded().connect( mEventSlotPriority, [&]( app::TouchEvent &event ) {
			propagateTouchesEnded( event );
		} ) );
	}

	if( options.mKeyboard ) {
		mEventConnections.push_back( mWindow->getSignalKeyDown().connect( mEventSlotPriority, [&]( app::KeyEvent &event ) {
			propagateKeyDown( event );
		} ) );
		mEventConnections.push_back( mWindow->getSignalKeyUp().connect( mEventSlotPriority, [&]( app::KeyEvent &event ) {
			propagateKeyUp( event );
		} ) );
	}
}

void Graph::disconnectEvents()
{
	for( auto &connection : mEventConnections )
		connection.disconnect();

	mEventConnections.clear();
}

void Graph::propagateTouchesBegan( app::TouchEvent &event )
{
	mCurrentTouchEvent = event;
	for( const auto &touch : event.getTouches() )
		mActiveTouches[touch.getId()] = touch;

	auto thisRef = shared_from_this();
	size_t numTouchesHandled = 0;
	propagateTouchesBegan( thisRef, event, numTouchesHandled );
}

void Graph::propagateTouchesBegan( ViewRef &view, app::TouchEvent &event, size_t &numTouchesHandled )
{
	if( view->isHidden() || ! view->isInteractive() )
		return;

	LOG_TOUCHES( view->getName() << " | num touches A: " << event.getTouches().size() );

	vector<app::TouchEvent::Touch> touchesInside;
	touchesInside.reserve( event.getTouches().size() );

	for( const auto &touch : event.getTouches() ) {
		vec2 pos = view->toLocal( touch.getPos() );
		if( view->hitTest( pos ) ) {
			touchesInside.push_back( touch );
		}
	}

	LOG_TOUCHES( view->getName() << " | num touchesInsde: " << touchesInside.size() );

	if( touchesInside.empty() )
		return;

	// TODO: this copy is currently necessary to prevent bad iterators if a view is added during iteration
	// - Might defer adding but need to think through how the ordering will be handled
	auto subviews = view->mSubviews;
	for( auto rIt = subviews.rbegin(); rIt != subviews.rend(); ++rIt ) {
		event.getTouches() = touchesInside; // TODO: find a way to avoid making this copy per subview
		propagateTouchesBegan( *rIt, event, numTouchesHandled );
		if( event.isHandled() )
			return;
	}

	event.getTouches() = touchesInside;

	if( view->touchesBegan( event ) ) {
		// Only allow this View to handle this touch in other UI events.
		auto &touches = event.getTouches();
		size_t numTouchesHandledThisView = 0;
		for( auto &touch : touches ) {
			if( touch.isHandled() ) {
				view->mActiveTouches[touch.getId()] = touch;
				numTouchesHandled++;
				numTouchesHandledThisView++;
			}
		}

		LOG_TOUCHES( view->getName() << " | numTouchesHandled: " << numTouchesHandled );

		// Remove active touches. Note: I'm having to do this outside of the above loop because I can't invalidate the vector::iterator
		touches.erase(
				remove_if( touches.begin(), touches.end(),
				           [&view]( const app::TouchEvent::Touch &touch ) {
					           if( touch.isHandled() ) {
						           LOG_TOUCHES( view->getName() << " | handled touch: " << touch.getId() );
					           }
					           return touch.isHandled();
				           } ),
				touches.end() );

		LOG_TOUCHES( view->getName() << " | num touches C: " << event.getTouches().size() );
		
		if( numTouchesHandledThisView != 0 && find( mViewsWithTouches.begin(), mViewsWithTouches.end(), view ) == mViewsWithTouches.end() ) {
			mViewsWithTouches.push_back( view );
		}

		if( numTouchesHandled == mCurrentTouchEvent.getTouches().size() ) {
			event.setHandled();
		}

		LOG_TOUCHES( "handled: " << event.isHandled() );
	}
}

void Graph::propagateTouchesMoved( app::TouchEvent &event )
{
	mCurrentTouchEvent = event;
	for( const auto &touch : event.getTouches() )
		mActiveTouches[touch.getId()] = touch;

//	size_t numTouchesHandled = 0;

	for( auto &view : mViewsWithTouches ) {
//		LOG_TOUCHES( view->getName() << " | num touches A: " << event.getTouches().size() );

		CI_ASSERT( ! view->mActiveTouches.empty() );
		// Update active touches
		vector<app::TouchEvent::Touch> touchesContinued;
		for( const auto &touch : mCurrentTouchEvent.getTouches() ) {
			auto touchIt = view->mActiveTouches.find( touch.getId() );
			if( touchIt == view->mActiveTouches.end() )
				continue;

			view->mActiveTouches[touch.getId()] = touch;
			touchesContinued.push_back( touch );
		}

//		LOG_TOUCHES( view->getName() << " | num touchesContinued: " << touchesContinued.size() );

		if( ! touchesContinued.empty() ) {
			event.getTouches() = touchesContinued;
			view->touchesMoved( event );

			// for now always updating the active touch in touch map
//			for( auto &touch : event.getTouches() ) {
//				if( touch.isHandled() ) {
//					numTouchesHandled++;
//					view->mActiveTouches.at( touch.getId() ) = touch;
//				}
//			}
		}
	}

//	if( numTouchesHandled == mCurrentTouchEvent.getTouches().size() ) {
//		event.setHandled();
//	}
//	LOG_TOUCHES( "handled: " << event.isHandled() );
}

void Graph::propagateTouchesEnded( app::TouchEvent &event )
{
	mCurrentTouchEvent = event;
//	size_t numTouchesHandled = 0;

	for( auto viewIt = mViewsWithTouches.begin(); viewIt != mViewsWithTouches.end(); /* */ ) {
		auto &view = *viewIt;
		LOG_TOUCHES( view->getName() << " | num active touches: " << view->mActiveTouches.size() );

		CI_ASSERT( ! view->mActiveTouches.empty() );
		// Update active touches
		vector<app::TouchEvent::Touch> touchesEnded;
		for( const auto &touch : mCurrentTouchEvent.getTouches() ) {
			auto touchIt = view->mActiveTouches.find( touch.getId() );
			if( touchIt == view->mActiveTouches.end() )
				continue;

			view->mActiveTouches[touch.getId()] = touch;
			touchesEnded.push_back( touch );
		}

		LOG_TOUCHES( view->getName() << " | num touchesEnded: " << touchesEnded.size() );

		if( ! touchesEnded.empty() ) {
			event.getTouches() = touchesEnded;
			view->touchesEnded( event );

			for( const auto &touch : touchesEnded ) {
				view->mActiveTouches.erase( touch.getId() );
			}
		}

		// remove View from container once all its active touches have ended
		if( view->mActiveTouches.empty() ) {
			viewIt = mViewsWithTouches.erase( viewIt );
		}
		else {
			++viewIt;
		}
	}

	for( const auto &touch : mCurrentTouchEvent.getTouches() ) {
		size_t numRemoved = mActiveTouches.erase( touch.getId() );
		//CI_VERIFY( numRemoved != 0 );
		if( numRemoved == 0 ) {
			CI_LOG_W( "stray touch attempted to be removed" );
		}
		LOG_TOUCHES( "touch: " << touch.getId() << ", num removed: " << numRemoved );
	}

	mCurrentTouchEvent.getTouches().clear();
}

void Graph::propagateKeyDown( ci::app::KeyEvent &event )
{
	auto thisRef = shared_from_this();
	propagateKeyDown( thisRef, event );
}

void Graph::propagateKeyUp( ci::app::KeyEvent &event )
{
	auto thisRef = shared_from_this();
	propagateKeyUp( thisRef, event );
}

void Graph::propagateKeyDown( ViewRef &view, ci::app::KeyEvent &event )
{
	if( view->isHidden() || ! view->isInteractive() )
		return;

	for( auto rIt = view->mSubviews.rbegin(); rIt != view->mSubviews.rend(); ++rIt ) {
		propagateKeyDown( *rIt, event );
		if( event.isHandled() )
			return;
	}

	if( view->keyDown( event ) ) {
		event.setHandled();
	}
}

void Graph::propagateKeyUp( ViewRef &view, ci::app::KeyEvent &event )
{
	if( view->isHidden() || ! view->isInteractive() )
		return;

	for( auto rIt = view->mSubviews.rbegin(); rIt != view->mSubviews.rend(); ++rIt ) {
		propagateKeyUp( *rIt, event );
		if( event.isHandled() )
			return;
	}

	if( view->keyUp( event ) ) {
		event.setHandled();
	}
}

} // namespace ui
