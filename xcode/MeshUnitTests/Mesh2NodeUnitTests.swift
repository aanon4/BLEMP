//
//  Mesh2NodeUnitTests.swift
//  Mesh
//
//  Created by tim on 1/17/15.
//  Copyright (c) 2015 tim. All rights reserved.
//

import XCTest

class Mesh2NodeUnitTests: XCTestCase {

  override func setUp() {
    super.setUp()
    simSetup(2)
    simDiscover()
  }
  
  override func tearDown() {
    XCTAssertEqual(simValidateState(), UInt8(1), "State invalid")
    super.tearDown()
  }

  func test00Setup() {
    simTicks(10)
  }
  
  func test01SimpleSet() {
    Helpers.setValue(&nodes.0, target: 0, key: 0x1234, version: 1, value: 1)
    
    simTicks(50)
  }
  
  func test02LocalSetRemoteGet() {
    // Set a local value on Node0
    Helpers.setValue(&nodes.0, target: 0, key: 0x1234, version: 1, value: 19)
    
    simTicks(50)
    
    // Check the value has arrived on Node1
    XCTAssertEqual(Helpers.getValue(&nodes.1, target: 0, key: 0x1234), 19, "Get value fail")
  }
  
  func test10VersionCollision() {
    Helpers.setValue(&nodes.0, target: 0, key: 0x1234, version: 1, value: 10)
    Helpers.setValue(&nodes.1, target: 0, key: 0x1234, version: 1, value: 11)
    
    simTicks(50)
    
    XCTAssertEqual(Helpers.getValue(&nodes.0, target: 0, key: 0x1234), 11, "Get value fail")
    XCTAssertEqual(Helpers.getValue(&nodes.1, target: 0, key: 0x1234), 11, "Get value fail")
  }
  
  func test10ValueCollision() {
    Helpers.setValue(&nodes.0, target: 0, key: 0x1234, version: 1, value: 100)
    simTicks(50)
    XCTAssertEqual(simValidateState(), UInt8(1), "State invalid")
    
    Helpers.setValue(&nodes.1, target: 0, key: 0x1234, version: 1, value: 50)
    simTicks(50)
    XCTAssertEqual(simValidateState(), UInt8(1), "State invalid")

    Helpers.setValue(&nodes.1, target: 0, key: 0x1234, version: 1, value: 110)
    simTicks(50)
    XCTAssertEqual(simValidateState(), UInt8(1), "State invalid")

    Helpers.setValue(&nodes.1, target: 0, key: 0x1234, version: 1, value: 80)
    simTicks(50)
    XCTAssertEqual(simValidateState(), UInt8(1), "State invalid")
    
    Helpers.setValue(&nodes.1, target: 0, key: 0x1234, version: 1, value: 115)
    simTicks(50)
    XCTAssertEqual(simValidateState(), UInt8(1), "State invalid")

    Helpers.setValue(&nodes.1, target: 0, key: 0x1234, version: 1, value: 10)
    simTicks(50)
    XCTAssertEqual(simValidateState(), UInt8(1), "State invalid")

    Helpers.setValue(&nodes.1, target: 0, key: 0x1234, version: 1, value: 20)
    simTicks(50)
    XCTAssertEqual(simValidateState(), UInt8(1), "State invalid")

    Helpers.setValue(&nodes.1, target: 0, key: 0x1234, version: 1, value: 120)
    simTicks(50)
    
    XCTAssertEqual(Helpers.getValue(&nodes.0, target: 0, key: 0x1234), 120, "Get value fail")
  }

}
