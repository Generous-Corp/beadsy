// Minimal CoreGraphics UI driver for live-process proof: window geometry,
// screenshots, real pointer clicks/drags (screen points, top-left origin).
import Foundation
import CoreGraphics
import AppKit

func windows(pid: Int) -> [[String: Any]] {
    let list = CGWindowListCopyWindowInfo([.optionAll], kCGNullWindowID) as! [[String: Any]]
    return list.filter { ($0["kCGWindowOwnerPID"] as? Int) == pid && ($0["kCGWindowLayer"] as? Int) == 0 }
}
func post(_ type: CGEventType, _ p: CGPoint, button: CGMouseButton) {
    let e = CGEvent(mouseEventSource: nil, mouseType: type, mouseCursorPosition: p, mouseButton: button)!
    e.post(tap: .cghidEventTap)
}
let args = CommandLine.arguments
switch args[1] {
case "windows":
    for w in windows(pid: Int(args[2])!) {
        let b = w["kCGWindowBounds"] as! [String: Any]
        print("\(w["kCGWindowNumber"]!) \(w["kCGWindowName"] ?? "") \(b["X"]!) \(b["Y"]!) \(b["Width"]!) \(b["Height"]!)")
    }
case "shot":
    let t = Process(); t.executableURL = URL(fileURLWithPath: "/usr/sbin/screencapture")
    t.arguments = ["-l", args[2], "-x", "-o", args[3]]; try! t.run(); t.waitUntilExit()
    print("screencapture exit \(t.terminationStatus) -> \(args[3])")
case "click", "rclick":
    let p = CGPoint(x: Double(args[2])!, y: Double(args[3])!)
    let right = args[1] == "rclick"
    let btn: CGMouseButton = right ? .right : .left
    post(.mouseMoved, p, button: .left); usleep(60000)
    post(right ? .rightMouseDown : .leftMouseDown, p, button: btn); usleep(70000)
    post(right ? .rightMouseUp : .leftMouseUp, p, button: btn); usleep(60000)
    print("clicked \(p) right=\(right)")
case "drag":
    let a = CGPoint(x: Double(args[2])!, y: Double(args[3])!)
    let b = CGPoint(x: Double(args[4])!, y: Double(args[5])!)
    let steps = args.count > 6 ? Int(args[6])! : 25
    post(.mouseMoved, a, button: .left); usleep(120000)
    post(.leftMouseDown, a, button: .left); usleep(120000)
    for i in 1...steps {
        let t = Double(i) / Double(steps)
        post(.leftMouseDragged, CGPoint(x: a.x + (b.x - a.x) * t, y: a.y + (b.y - a.y) * t), button: .left)
        usleep(20000)
    }
    usleep(120000)
    post(.leftMouseUp, b, button: .left); usleep(120000)
    print("dragged \(a) -> \(b)")
case "screen":
    for sc in NSScreen.screens { print("frame=\(sc.frame) visible=\(sc.visibleFrame) scale=\(sc.backingScaleFactor)") }
case "pixel":
    // pixel FILE X Y [X Y ...] -> rgba at integer pixel coords (top-left origin)
    let img = NSImage(contentsOfFile: args[2])!
    let rep = NSBitmapImageRep(data: img.tiffRepresentation!)!
    var i = 3
    while i + 1 < args.count {
        let c = rep.colorAt(x: Int(args[i])!, y: Int(args[i+1])!)!
        print("(\(args[i]),\(args[i+1])) r=\(Int(c.redComponent*255)) g=\(Int(c.greenComponent*255)) b=\(Int(c.blueComponent*255))")
        i += 2
    }
case "centroid":
    // centroid FILE X0 Y0 X1 Y1 THRESH -> centroid + bbox of pixels with r+g+b < THRESH
    let img = NSImage(contentsOfFile: args[2])!
    let rep = NSBitmapImageRep(data: img.tiffRepresentation!)!
    let x0 = Int(args[3])!, y0 = Int(args[4])!, x1 = Int(args[5])!, y1 = Int(args[6])!, th = Int(args[7])!
    var sx = 0.0, sy = 0.0, n = 0, minx = Int.max, maxx = -1, miny = Int.max, maxy = -1
    for y in y0..<y1 { for x in x0..<x1 {
        let c = rep.colorAt(x: x, y: y)!
        let v = Int((c.redComponent + c.greenComponent + c.blueComponent) * 255)
        if v < th { sx += Double(x); sy += Double(y); n += 1; minx = min(minx, x); maxx = max(maxx, x); miny = min(miny, y); maxy = max(maxy, y) }
    } }
    if n > 0 { print(String(format: "n=%d centroid=(%.1f,%.1f) bbox=(%d,%d)-(%d,%d) mid=(%.1f,%.1f)", n, sx/Double(n), sy/Double(n), minx, miny, maxx, maxy, Double(minx+maxx)/2, Double(miny+maxy)/2)) } else { print("n=0") }
case "cols":
    // cols FILE X0 Y0 X1 Y1 THRESH -> per-column count of dark pixels (non-zero only)
    let img = NSImage(contentsOfFile: args[2])!
    let rep = NSBitmapImageRep(data: img.tiffRepresentation!)!
    let x0 = Int(args[3])!, y0 = Int(args[4])!, x1 = Int(args[5])!, y1 = Int(args[6])!, th = Int(args[7])!
    var out: [String] = []
    for x in x0..<x1 { var n = 0; for y in y0..<y1 { let c = rep.colorAt(x: x, y: y)!; if Int((c.redComponent + c.greenComponent + c.blueComponent) * 255) < th { n += 1 } }; if n > 0 { out.append("\(x):\(n)") } }
    print(out.joined(separator: " "))
case "rows":
    let img = NSImage(contentsOfFile: args[2])!
    let rep = NSBitmapImageRep(data: img.tiffRepresentation!)!
    let x0 = Int(args[3])!, y0 = Int(args[4])!, x1 = Int(args[5])!, y1 = Int(args[6])!, th = Int(args[7])!
    var out: [String] = []
    for y in y0..<y1 { var n = 0; for x in x0..<x1 { let c = rep.colorAt(x: x, y: y)!; if Int((c.redComponent + c.greenComponent + c.blueComponent) * 255) < th { n += 1 } }; if n > 0 { out.append("\(y):\(n)") } }
    print(out.joined(separator: " "))
case "colorc":
    // colorc FILE X0 Y0 X1 Y1 RMIN RMAX GMIN GMAX BMIN BMAX -> centroid of pixels in the colour box
    let img = NSImage(contentsOfFile: args[2])!
    let rep = NSBitmapImageRep(data: img.tiffRepresentation!)!
    let x0 = Int(args[3])!, y0 = Int(args[4])!, x1 = Int(args[5])!, y1 = Int(args[6])!
    let rmin = Int(args[7])!, rmax = Int(args[8])!, gmin = Int(args[9])!, gmax = Int(args[10])!, bmin = Int(args[11])!, bmax = Int(args[12])!
    var sx = 0.0, sy = 0.0, n = 0, minx = Int.max, maxx = -1, miny = Int.max, maxy = -1
    for y in y0..<y1 { for x in x0..<x1 {
        let c = rep.colorAt(x: x, y: y)!
        let r = Int(c.redComponent*255), g = Int(c.greenComponent*255), b = Int(c.blueComponent*255)
        if r >= rmin && r <= rmax && g >= gmin && g <= gmax && b >= bmin && b <= bmax { sx += Double(x); sy += Double(y); n += 1; minx = min(minx, x); maxx = max(maxx, x); miny = min(miny, y); maxy = max(maxy, y) }
    } }
    if n > 0 { print(String(format: "n=%d centroid=(%.1f,%.1f) bbox=(%d,%d)-(%d,%d) mid=(%.1f,%.1f)", n, sx/Double(n), sy/Double(n), minx, miny, maxx, maxy, Double(minx+maxx)/2, Double(miny+maxy)/2)) } else { print("n=0") }
case "move":
    post(.mouseMoved, CGPoint(x: Double(args[2])!, y: Double(args[3])!), button: .left); print("moved")
default: print("usage: windows PID | shot WID OUT | click X Y | rclick X Y | drag X1 Y1 X2 Y2 [steps] | move X Y")
}
