
using System;
using System.Security.Cryptography;
using System.IO;
namespace cryptlib
{


	public class AES
	{
		static int key_bits = 256;
		public static int total_key_size = key_bits/8 + 16;
		public AES ()
		{

		}
		public static byte[] newAESKey()
		{
			RijndaelManaged r = new RijndaelManaged();
			r.KeySize = key_bits;
			r.GenerateKey();
			r.GenerateIV();
			byte[] k = r.Key;
			byte[] i = r.IV;
			byte[] total = new byte[k.Length + i.Length];
			k.CopyTo(total,0);
			i.CopyTo(total,k.Length);
			return total;
		}
		static ICryptoTransform fetchEncryptor(byte[] totalKey)
		{
			RijndaelManaged r = new RijndaelManaged();
				
			byte[] key = new byte[key_bits / 8];
			byte[] iv = new byte[16];
			int i;
			for(i = 0; i < key_bits/8; i++)
			{
				key[i]=totalKey[i];
			}
			for(int j = 0; j < 16; j++)
			{
				//Console.WriteLine("r.iv[{0}]=totalKey[{1}]",j,i);
				iv[j] = totalKey[i++];
			}
			return r.CreateEncryptor(key,iv);
		}
				static ICryptoTransform fetchDecryptor(byte[] totalKey)
		{
			RijndaelManaged r = new RijndaelManaged();
				
			byte[] key = new byte[key_bits / 8];
			byte[] iv = new byte[16];
			int i;
			for(i = 0; i < key_bits/8; i++)
			{
				key[i]=totalKey[i];
			}
			for(int j = 0; j < 16; j++)
			{
				//Console.WriteLine("r.iv[{0}]=totalKey[{1}]",j,i);
				iv[j] = totalKey[i++];
			}
			return r.CreateDecryptor(key,iv);
		}
		public static byte[] encrypt (byte[] plaintext,int length,byte[] totalKey)
		{
			ICryptoTransform e = fetchEncryptor(totalKey);
			MemoryStream ms = new MemoryStream();
			CryptoStream cs = new CryptoStream(ms,e,CryptoStreamMode.Write);
			cs.Write(plaintext,0,length);
			cs.FlushFinalBlock();
			return ms.ToArray();
		}
		public static byte[] decrypt(byte[] ciphertext, byte[] totalKey)
		{

			ICryptoTransform d = fetchDecryptor(totalKey);
			MemoryStream ms = new MemoryStream(ciphertext);
			CryptoStream cs = new CryptoStream(ms,d,CryptoStreamMode.Read);
			byte[] plainText = new byte[ciphertext.Length];
			int count = cs.Read(plainText,0,plainText.Length);
			byte[] realPlainText = new byte[count];
			Array.Copy(plainText,realPlainText,count);
			return realPlainText;

		}
		
	}
}
