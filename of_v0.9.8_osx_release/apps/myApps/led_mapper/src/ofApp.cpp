#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup(){
    // Input
    cam.setup(640, 480);
    
    // GUI
    gui.setup();
    gui.add(resetBackground.set("Reset Background", false));
    gui.add(learningTime.set("Learning Time", 30, 0, 30));
    gui.add(thresholdValue.set("Threshold Value", 10, 0, 255));
    
    // Contours
    contourFinder.setMinAreaRadius(1);
    contourFinder.setMaxAreaRadius(100);
    contourFinder.setThreshold(15);
    // wait for half a frame before forgetting something
    contourFinder.getTracker().setPersistence(15);
    // an object can move up to 32 pixels per frame
    contourFinder.getTracker().setMaximumDistance(32);
    
    showLabels = true;
    
    // LED
    ofSetFrameRate(10);
    
    ledIndex = 0;
    numLeds = 50;
    ledBrightness = 127;
    isMapping = false;
    
    // Set up the color vector, with all LEDs set to off(black)
    pixels.assign(numLeds, ofColor(0,0,0));
    setAllLEDColours(ofColor(0, 0,0));
    
    // Connect to the fcserver
    opcClient.setup("192.168.0.211", 7890);
    
}

//--------------------------------------------------------------
void ofApp::update(){
    opcClient.update();
    
    // If the client is not connected do not try and send information
    if (!opcClient.isConnected()) {
        // Will continue to try and reconnect to the Pixel Server
        opcClient.tryConnecting();
    }
    
    cam.update();
    if(resetBackground) {
        background.reset();
        resetBackground = false;
    }
    if(cam.isFrameNew()) {
        // Light up a new LED for every frame
        if (isMapping) {
            chaseAnimation();
        }
        // Background subtraction
        background.setLearningTime(learningTime);
        background.setThresholdValue(thresholdValue);
        background.update(cam, thresholded);
        thresholded.update();
        
        // Contour
        ofxCv::blur(thresholded, 10);
        contourFinder.findContours(thresholded);
        
    }
}

//--------------------------------------------------------------
void ofApp::draw(){
    cam.draw(0, 0);
    if(thresholded.isAllocated()) {
        thresholded.draw(640, 0);
    }
    gui.draw();
    
    ofSetBackgroundAuto(showLabels);
    ofxCv::RectTracker& tracker = contourFinder.getTracker();
    
    if(showLabels) {
        ofSetColor(255);
        //movie.draw(0, 0);
        contourFinder.draw();
        for(int i = 0; i < contourFinder.size(); i++) {
            ofPoint center = ofxCv::toOf(contourFinder.getCenter(i));
            // Store centroids for drawing/saving
            centroids.push_back(center);
            ofPushMatrix();
            ofTranslate(center.x, center.y);
            int label = contourFinder.getLabel(i);
            string msg = ofToString(label) + ":" + ofToString(tracker.getAge(label));
            ofDrawBitmapString(msg, 0, 0);
            ofVec2f velocity = ofxCv::toOf(contourFinder.getVelocity(i));
            ofScale(5, 5);
            ofDrawLine(0, 0, velocity.x, velocity.y);
            ofPopMatrix();
        }
    } else {
        for(int i = 0; i < contourFinder.size(); i++) {
            unsigned int label = contourFinder.getLabel(i);
            // only draw a line if this is not a new label
            if(tracker.existsPrevious(label)) {
                // use the label to pick a random color
                ofSeedRandom(label << 24);
                ofSetColor(ofColor::fromHsb(ofRandom(255), 255, 255));
                // get the tracked object (cv::Rect) at current and previous position
                const cv::Rect& previous = tracker.getPrevious(label);
                const cv::Rect& current = tracker.getCurrent(label);
                // get the centers of the rectangles
                ofVec2f previousPosition(previous.x + previous.width / 2, previous.y + previous.height / 2);
                ofVec2f currentPosition(current.x + current.width / 2, current.y + current.height / 2);
                ofDrawLine(previousPosition, currentPosition);
            }
        }
    }
    for (int i = 0; i < centroids.size(); i++) {
        ofDrawCircle(centroids[i].x, centroids[i].y, 3);
    }
    
    // this chunk of code visualizes the creation and destruction of labels
    const vector<unsigned int>& currentLabels = tracker.getCurrentLabels();
    const vector<unsigned int>& previousLabels = tracker.getPreviousLabels();
    const vector<unsigned int>& newLabels = tracker.getNewLabels();
    const vector<unsigned int>& deadLabels = tracker.getDeadLabels();
    ofSetColor(ofxCv::cyanPrint);
    for(int i = 0; i < currentLabels.size(); i++) {
        int j = currentLabels[i];
        ofDrawLine(j, 0, j, 4);
    }
    ofSetColor(ofxCv::magentaPrint);
    for(int i = 0; i < previousLabels.size(); i++) {
        int j = previousLabels[i];
        ofDrawLine(j, 4, j, 8);
    }
    ofSetColor(ofxCv::yellowPrint);
    for(int i = 0; i < newLabels.size(); i++) {
        int j = newLabels[i];
        ofDrawLine(j, 8, j, 12);
    }
    ofSetColor(ofColor::white);
    for(int i = 0; i < deadLabels.size(); i++) {
        int j = deadLabels[i];
        ofDrawLine(j, 12, j, 16);
    }
    
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
    switch (key){
        case ' ':
            centroids.clear();
            break;
        case '+':
            threshold ++;
            cout << "Threshold: " << threshold;
            if (threshold > 255) threshold = 255;
            break;
        case '-':
            threshold --;
            cout << "Threshold: " << threshold;
            if (threshold < 0) threshold = 0;
            break;
        case 's':
            isMapping = true;
            break;
    }

}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){ 

}

// Cycle through all LEDs, return false when done
void ofApp::chaseAnimation()
{
    // Chase animation
    for (int i = 0; i <  numLeds; i++) {
        ofColor col;
        if (i == ledIndex) {
            col = ofColor(ledBrightness, ledBrightness, ledBrightness);
        }
        else {
            col = ofColor(0, 0, 0);
        }
        pixels.at(i) = col;
    }
    
    opcClient.writeChannel(1, pixels);
    
    ledIndex++;
    if (ledIndex >= numLeds) {
        ledIndex = 0;
        setAllLEDColours(ofColor(0, 0, 0));
        isMapping = false;
    }
    
}

// Set all LEDs to the same colour (useful to turn them all on or off).
void ofApp::setAllLEDColours(ofColor col) {
    // Chase animation
    for (int i = 0; i <  numLeds; i++) {
        pixels.at(i) = col;
    }
    opcClient.writeChannel(1, pixels);
}