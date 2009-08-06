
using System;

namespace logger
{

	/// <summary>
	/// Better than Console.WriteLine()...
	/// </summary>
	public class logger
	{
		static System.Collections.Generic.Stack<string> stack = new System.Collections.Generic.Stack<string>();
		public static void push(string msg)
		{
			lock(stack)
			{
			stack.Push(msg);
			}
		}
		public static void pop()
		{
			lock(stack)
			{
			stack.Pop();
			}
		}
		static object ilock = new object();
		public static void debug(string msg)
		{
			//lock(ilock)
			//{
			setColor(ConsoleColor.White,ConsoleColor.Green);
			log("DEBUG",msg,System.Reflection.Assembly.GetCallingAssembly().GetName().Name,Console.Error);
			resetColor();
			//}
		}
		public static void machineReadable(string msg, string machineID)
		{
			//lock(ilock){
			resetColor();
			log("MACHINEREADABLE","<"+machineID+">"+msg+"</"+machineID+">",System.Reflection.Assembly.GetCallingAssembly().GetName().Name,Console.Out);
			//}
		}
		public static void setColor(ConsoleColor fg, ConsoleColor bg)
		{
			try
			{
				Console.BackgroundColor = bg;
				Console.ForegroundColor = fg;
			}
			catch {}
		}
		public static void resetColor()
		{
			try
			{
				Console.ResetColor();
			}
			catch {}
		}
		public static void warn(string msg)
		{
			//lock(ilock)
			//{
			setColor(ConsoleColor.DarkBlue,ConsoleColor.DarkYellow);
			log("WARNING",msg,System.Reflection.Assembly.GetCallingAssembly().GetName().Name,Console.Error);
			resetColor();
			//}
		}
		public static void info(string msg)
		{
			//lock(ilock)
		//	{
			
			setColor(ConsoleColor.Black,ConsoleColor.White);
			log("INFO",msg,System.Reflection.Assembly.GetCallingAssembly().GetName().Name,Console.Error);
			resetColor();
			//}
		}
		public static void FAIL(string msg)
		{
			//lock(ilock)
			//{
			setColor(ConsoleColor.White,ConsoleColor.DarkRed);
			log("FAIL",msg,System.Reflection.Assembly.GetCallingAssembly().GetName().Name,Console.Error);
			resetColor();
			//}
			System.Environment.Exit(1);
		}
		static void log(string TYPE, string MSG, string CALLER, System.IO.TextWriter whereTo)
		{
			DateTime d = DateTime.Now;
			string appName = System.Reflection.Assembly.GetEntryAssembly().GetName().Name;
			string total = "";
			lock(stack)
			{
			foreach(string s in stack)
			{
				total += s + "-";
			}
			}
			whereTo.WriteLine("{0} {1} {2}:{3} {4} {5}",d,TYPE,appName,CALLER,total,MSG);
			whereTo.Flush();
		}
	}
}
