//
//  Mesh1NodeUnitTests.swift
//  Mesh
//
//  Created by tim on 1/17/15.
//  Copyright (c) 2015 tim. All rights reserved.
//

import XCTest

class Mesh1NodeUnitTests: XCTestCase {

  override func setUp() {
    super.setUp()
    simSetup(1)
    simDiscover()
  }
  
  override func tearDown() {
    XCTAssertEqual(simValidateState(), UInt8(1), "State invalid")
    super.tearDown()
  }

  func test00Setup() {
  }

  func test01SimpleSet() {
    Helpers.setValue(&nodes.0, target: 0, key: 0x1234, version: 1, value: 1)
  }
  
  func test02SimpleSetGet() {
    Helpers.setValue(&nodes.0, target: 0, key: 0x1234, version: 1, value: 1)
    XCTAssertEqual(Helpers.getValue(&nodes.0, target: 0, key: 0x1234), 1, "Get value fail")
  }

}
