// Compiled using the official Zig 0.16.0 distribution. This deliberately uses
// standard library I/O, rather than a handcrafted NtWriteFile reproduction.
const std = @import("std");
pub fn main(init: std.process.Init) !void {
    var buffer: [128]u8 = undefined;
    var writer: std.Io.File.Writer = .init(.stdout(), init.io, &buffer);
    try writer.interface.writeAll("KxNt Zig standard-library console smoke\n");
    try writer.interface.flush();
}
