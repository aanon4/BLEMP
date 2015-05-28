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
    var data: [UInt8] = [1]
    var result = MESH_OK
    withUnsafeMutablePointer(&data[0]) { dataPtr -> Void in
      result = Mesh_SetValueInternal(&nodes.0, 0, 0x1234, dataPtr, UInt8(data.count), 1, 1, 1)
    }
    XCTAssertEqual(result.value, MESH_OK.value, "Set fail")
  }
  
  func test02SimpleSetGet() {
    var data: [UInt8] = [1]
    var result = MESH_OK
    withUnsafeMutablePointer(&data[0]) { dataPtr -> Void in
      result = Mesh_SetValueInternal(&nodes.0, 0, 0x1234, dataPtr, UInt8(data.count), 1, 1, 1)
    }
    XCTAssertEqual(result.value, MESH_OK.value, "Set fail")
    
    var gdata: [UInt8] = [0]
    withUnsafeMutablePointer(&gdata[0]) { gdataPtr -> Void in
      var count = UInt8(gdata.count)
      result = Mesh_GetValue(&nodes.0, 0, 0x1234, gdataPtr, &count)
    }
    XCTAssertEqual(result.value, MESH_OK.value, "Get fail")
    XCTAssertEqual(gdata[0], UInt8(1), "Get value fail")
  }

}
