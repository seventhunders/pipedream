
using System;
using System.Collections.Generic;
namespace pipette
{

	/// <summary>
	/// Simple message-escape protocol for streams.  Not for use as file transfer--too slow.
	/// </summary>
	public class msgBase
	{
		static byte special_reserved = 0;
		static byte terminates = 1;
		static byte non_terminates = 2;
		public msgBase ()
		{
		}
		public static void write(System.IO.Stream s, byte[] msg)
		{
			logger.logger.debug("Sending msg:");
			foreach(byte q in msg)
			{
				Console.Write(q);
			}
			Console.WriteLine();
			List<byte> b = new List<byte>();
			foreach(byte a in msg)
			{
				b.Add(a);
				if (a==special_reserved)
				{
					b.Add(non_terminates); //double-escape
				}
			}
			b.Add(special_reserved);
			b.Add(terminates);
			logger.logger.debug(b[b.Count - 2].ToString());
			logger.logger.debug(b[b.Count - 1].ToString());


			s.Write(b.ToArray(),0,b.Count);
		}
		public static byte[] read(System.IO.Stream s, deepID d)
		{
			List<byte> b = new List<byte>();
			bool reserve_flag = false;
			while(true)
			{
				if (b.Count > (1024*1024))
				{
					logger.logger.warn("Message > 1MB.  Pretending like the client disconnected, since this data is obviously invalid.");
					return new byte[0];
				}
				//logger.logger.debug("msgbase reading...");
				byte a = (byte) s.ReadByte();
				if (a==-1) d.raiseReadZeroBytes();
				if (a==special_reserved)
				{
					logger.logger.debug("HI!");
					if (!reserve_flag) reserve_flag = true;
				}
				else if (reserve_flag && a==terminates) 
				{
					logger.logger.debug("Returning " + b.Count + " bytes...");
					foreach(byte q in b)
					{
						Console.Write(q);
					}
					Console.WriteLine();
					return b.ToArray();
				}
				else if (reserve_flag && a==non_terminates) 
				{
					logger.logger.debug("here");

					reserve_flag = false;
					b.Add(special_reserved);

				}
				else if (reserve_flag) 
				{
					throw new Exception("Doesn't follow msgBase protocol!");
				}
				else b.Add(a);
				

			}
		}
	}
}
