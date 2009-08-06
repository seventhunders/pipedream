
using System;
using System.Security.Cryptography;
namespace cryptlib
{

	public class AsymmetricKey
	{
		public string privateKey;
		public string publicKey;
	}
	public class RSA
	{

		public static AsymmetricKey randomRSAKey()
		{
			RSACryptoServiceProvider r = new RSACryptoServiceProvider(2048);
			AsymmetricKey a = new AsymmetricKey();
			a.privateKey = r.ToXmlString(true);
			a.publicKey = r.ToXmlString(false);
			return a;
		}
		public static byte[] signDataWithKey(byte[] data, string xmlKey)
		{
			RSACryptoServiceProvider r = new RSACryptoServiceProvider();
			r.FromXmlString(xmlKey);
			return r.SignData(data,new System.Security.Cryptography.SHA1Managed());
		}
		public static bool verifyDataWithKey(byte[] data, string xmlKey, byte[] signature)
		{
			RSACryptoServiceProvider r = new RSACryptoServiceProvider();
			r.FromXmlString(xmlKey);
			return r.VerifyData(data,new System.Security.Cryptography.SHA1Managed(),signature);
		}
		/// <summary>
		/// 
		/// </summary>
		/// <param name="data">
		/// A <see cref="System.Byte[]"/>
		/// </param>
		/// <param name="xmlKey">
		/// A <see cref="System.String"/>
		/// Public key to encrypt with.  Private key not required with this method.
		/// </param>
		/// <returns>
		/// A <see cref="System.Byte[]"/>
		/// </returns>
		public static byte[] encrypt(byte[] data, string xmlKey)
		{
			RSACryptoServiceProvider r = new RSACryptoServiceProvider();
			r.FromXmlString(xmlKey);
			return r.Encrypt(data,true);
		}
		/// <summary>
		/// 
		/// </summary>
		/// <param name="data">
		/// A <see cref="System.Byte[]"/>
		/// </param>
		/// <param name="xmlKey">
		/// A <see cref="System.String"/>
		/// Private key to encrypt with.  If you don't pass the private key, an exception will be raised.
		/// </param>
		/// <returns>
		/// A <see cref="System.Byte[]"/>
		/// </returns>
		public static byte[] decrypt(byte[] data, string xmlKey)
		{
			RSACryptoServiceProvider r = new RSACryptoServiceProvider();
			r.FromXmlString(xmlKey);
			return r.Decrypt(data,true);
		}
	}
}
