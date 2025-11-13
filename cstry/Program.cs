using System;
using System.Runtime.InteropServices;
class Program
{
    [DllImport("C:\\Code\\Cat2\\x64\\Release\\Client\\Dog.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern int RunProgram(
        int argc,
        [MarshalAs(UnmanagedType.LPArray, ArraySubType = UnmanagedType.LPStr)] string[] argv
    );

    static void Main(string[] args)
    {
        string[] realargs = { "C:\\Code\\Cat2\\Dog", "$DevBuild$" };
        int result = RunProgram(realargs.Length, realargs);
        Console.WriteLine($"Returned: {result}");
    }
}
