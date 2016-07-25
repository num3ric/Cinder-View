/*
 Copyright (c) 2015, Richard Eakin - All rights reserved.

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

#include "ui/Suite.h"

#include "cinder/app/App.h"
#include "cinder/Log.h"
#include "cinder/audio/Context.h"

#include "cppformat/format.h"
#include "glm/gtc/epsilon.hpp"

using namespace std;
using namespace ci;

namespace ui {

const vec2 INFO_ROW_SIZE    = vec2( 200, 20 );
const float PADDING         = 6;

// ----------------------------------------------------------------------------------------------------
// Suite
// ----------------------------------------------------------------------------------------------------

Suite::Suite()
{
	mGraph = make_shared<ui::Graph>();
	mGraph->setLabel( "Suite root" );
	mGraph->setFillParentEnabled();
	mGraph->connectEvents();

	mSelector = make_shared<ui::VSelector>();
	mSelector->getBackground()->setColor( ColorA::gray( 0, 0.3f ) );
	mSelector->getSignalValueChanged().connect( [this] {
		selectTest( mSelector->getSelectedLabel() );
	} );

	mInfoLabel = make_shared<ui::LabelGrid>();
	mInfoLabel->setTextColor( Color::white() );
	mInfoLabel->getBackground()->setColor( ColorA::gray( 0, 0.3f ) );

	mGraph->addSubviews( { mSelector, mInfoLabel } );

	app::getWindow()->getSignalResize().connect( bind( &Suite::resize, this ) );
}

void Suite::resize()
{
	mGraph->setNeedsLayout();

	const float width = 120; // TODO: calculate widest segment
	const float height = 22 * mSelector->getSegmentLabels().size();
	const float windowWidth = app::getWindowWidth();
	mSelector->setBounds( Rectf( windowWidth - width - PADDING, PADDING, windowWidth - PADDING, height + PADDING ) );

	resizeInfoLabel();
}

void Suite::resizeInfoLabel()
{
	const int numRows = mInfoLabel->getNumRows();
	vec2 windowSize = vec2( app::getWindow()->getSize() );
	vec2 labelSize = { INFO_ROW_SIZE.x, INFO_ROW_SIZE.y * numRows };
	mInfoLabel->setBounds( { windowSize - labelSize - PADDING, windowSize - PADDING } ); // anchor bottom right
}

void Suite::selectTest( const string &key )
{
	CI_LOG_V( "selecting test: " << key ); // TODO: register signal instead, move log to test

	// first remove and destroy the current test
	if( mCurrentSuiteView ) {
		mCurrentSuiteView->removeFromParent();
		mCurrentSuiteView.reset();
	}

	mInfoLabel->clearCells();

	auto suiteView = mFactory.build( key );
	if( ! suiteView ) {
		CI_LOG_E( "no registered test for key: " << key );
		return;
	}

	suiteView->mSuite = this;
	suiteView->setFillParentEnabled();
	suiteView->setLabel( key );
	mGraph->insertSubview( suiteView, 0 );
	mCurrentSuiteView = suiteView;
	mCurrentTestKey = key;
}

void Suite::select( size_t index )
{
	mSelector->select( index );
}

void Suite::setDrawUiEnabled( bool enable )
{
	mDrawUi = enable;

	mInfoLabel->setHidden( ! enable );
	mSelector->setHidden( ! enable );
}

void Suite::reload()
{
	selectTest( mSelector->getSelectedLabel() );
}

void Suite::update()
{
	// select the first test if there has been one added to the selector and there is no current test view
	if( ! mCurrentSuiteView && ! mSelector->getSegmentLabels().empty() )
		selectTest( mSelector->getSelectedLabel() );

	updateUI();

	mGraph->propagateUpdate();
}

void Suite::updateUI()
{
	mInfoLabel->setRow( 0, { "fps:",  fmt::format( "{}", app::App::get()->getAverageFps() ) } );

	if( ! glm::epsilonEqual( INFO_ROW_SIZE.y * mInfoLabel->getNumRows(), mInfoLabel->getHeight(), 0.01f ) )
		resizeInfoLabel();
}

void Suite::draw()
{
	mGraph->propagateDraw();
}

} // namespace ui
