
using System;
using System.Security.Cryptography;
namespace cryptlib
{


	public class cryptLib
	{

		public cryptLib ()
		{
		}
		public static string randomString()
		{
			byte[] rByte = new byte[32];
			RNGCryptoServiceProvider p = new RNGCryptoServiceProvider();
			p.GetBytes(rByte);
			for(int i = 0; i < rByte.Length; i++)
			{
				rByte[i] =  (byte)((i % 95) + 32); //better ascii range
			}
			return System.Text.Encoding.ASCII.GetString(rByte);
		}
	}
}
