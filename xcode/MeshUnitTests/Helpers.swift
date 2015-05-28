//
//  Helpers.swift
//  Mesh
//
//  Created by tim on 1/17/15.
//  Copyright (c) 2015 tim. All rights reserved.
//

import XCTest

class Helpers {
  
  class func setValue(inout source: Mesh_Node, target: Int, key: Mesh_Key, version: Mesh_Version, value: Int) {
    // Set a local value on Node0
    var data: [UInt8] = [UInt8(value)]
    var result = MESH_OK
    withUnsafeMutablePointer(&data[0]) { dataPtr -> Void in
      var dataSpace = UnsafeMutablePointer<UInt8>.alloc(1)
      dataSpace.assignFrom(dataPtr, count: 1)
      var targetId = Mesh_NodeAddress(address: (UInt8(target + 1), 0, 0, 0, 0, 0))
      result = Mesh_SetValueInternal(&source, Mesh_InternNodeId(&source, &targetId, 1), key, dataSpace, UInt8(data.count), 1, version, simChangeBits(&source))
      simSync(&source)
    }
    XCTAssertEqual(result.value, MESH_OK.value, "Mesh_SetValue fail")
  }
  
  class func getValue(inout source: Mesh_Node, target: Int, key: Mesh_Key) -> Int {
    var result = MESH_OK
    var gdata: [UInt8] = [0]
    withUnsafeMutablePointer(&gdata[0]) { gdataPtr -> Void in
      var id = Mesh_NodeAddress(address: (UInt8(target + 1), 0, 0, 0, 0, 0))
      var count = UInt8(gdata.count)
      result = Mesh_GetValue(&source, Mesh_InternNodeId(&source, &id, 1), key, gdataPtr, &count)
    }
    XCTAssertEqual(result.value, MESH_OK.value, "Get fail")
    return Int(gdata[0])
  }
  
}