//
//  Mesh16NodeUnitTests.swift
//  Mesh16NodeUnitTests
//
//  Created by tim on 1/16/15.
//  Copyright (c) 2015 tim. All rights reserved.
//

import XCTest

class Mesh16NodeUnitTests: XCTestCase {
    
  override func setUp() {
    super.setUp()
    simSetup(16)
    simDiscover()
  }
  
  override func tearDown() {
    XCTAssertEqual(simValidateState(), UInt8(1), "State invalid")
    super.tearDown()
  }
    
  func test00Setup() {
    simTicks(100)
  }
  
  func test01SimpleSet() {    
    Helpers.setValue(&nodes.0, target: 0, key: 0x1234, version: 1, value: 1)
    
    simTicks(900)
  }
  
  func test02DoubleSet() {
    Helpers.setValue(&nodes.0, target: 0, key: 0x1234, version: 1, value: 1)
    simTicks(300)
    Helpers.setValue(&nodes.0, target: 0, key: 0x1235, version: 1, value: 2)
    simTicks(700)
  }
  
  func test03LocalSetRemoteGet() {
    // Set a local value on Node0
    Helpers.setValue(&nodes.0, target: 0, key: 0x1234, version: 1, value: 1)
    
    simTicks(300)
    
    // Check the value has arrived on Node8
    XCTAssertEqual(Helpers.getValue(&nodes.8, target: 0, key: 0x1234), 1, "Get value fail")
  }
  
  func test20Complex1() {
    simTicks(5)
    Helpers.setValue(&nodes.0, target: 0, key: 0x1234, version: 1, value: 0x55)
    simTicks(15)
    Helpers.setValue(&nodes.1, target: 0, key: 0x1234, version: 2, value: 0x22)
    Helpers.setValue(&nodes.1, target: 1, key: 0x1235, version: 1, value: 0x88)
    simTicks(80)
    Helpers.setValue(&nodes.0, target: 0, key: 0x1234, version: 3, value: 0x43)
    simTicks(100)
    Helpers.setValue(&nodes.13, target: 0, key: 0x1235, version: 2, value: 0xAA)
    simTick()
    Helpers.setValue(&nodes.14, target: 0, key: 0x1235, version: 3, value: 0xAB)
    simTick()
    Helpers.setValue(&nodes.15, target: 0, key: 0x1235, version: 4, value: 0xAC)
    simTicks(98)
    Helpers.setValue(&nodes.0, target: 8, key: 0x1235, version: 1, value: 0x01)
    Helpers.setValue(&nodes.4, target: 8, key: 0x1235, version: 1, value: 0x02)
    Helpers.setValue(&nodes.8, target: 8, key: 0x1235, version: 1, value: 0x03)
    Helpers.setValue(&nodes.15, target: 8, key: 0x1235, version: 1, value: 0x04)
    simTicks(400)
    XCTAssertEqual(Helpers.getValue(&nodes.0, target: 8, key: 0x1235), 4, "Get value fail")
  }
  
}
