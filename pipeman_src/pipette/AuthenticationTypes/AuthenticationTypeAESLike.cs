
using System;
using System.Collections.Generic;
namespace pipette
{

	/// <summary>
	/// An authentication type that uses AES for writeBuffer/readBuffer traffic
	/// Very fast.  Handle with care.
	/// </summary>
	public abstract class AuthenticationTypeAESLike: AuthenticationType
	{
		//cake-is-a-lie msgs
		 public const int remote_end_hungup = -1;
		 public const int remote_end_resumed = -2;
		
		private void handle_cake_is_a_lie_msg(int msg, deepID who)
		{
			switch(msg)
			{
			case remote_end_hungup:
				who.raiseCakeIsALie_remoteEndHungup();
				break;
			default:
				logger.logger.FAIL("I got this cakeisaliemsg: " + msg);
				break;
			}
		}
		
		static int max_msg_size = 8192;
		static int int_32_size = 32/8;
		public AuthenticationTypeAESLike (bool iChopYourDollar): base(iChopYourDollar)
		{
		}
		object inUse = new object();
		void logArray(byte[] b,string msg)
		{
			
			lock(inUse)
			{
				logger.logger.debug(msg+ ": " + b.Length);
				int i = 0;
				int q = 0;
				foreach(byte a in b)
				{
					q+=a;
					i++;
					q -= i;
					//logger.logger.debug("["+i++ +"] "+a.ToString());
				}
				logger.logger.debug("hash: " + q);
				
			}
		}
		public override byte[] readBuffer (deepID d)
		{
			int msg_size = readSize (d);
			if (msg_size < 0)
			{
				handle_cake_is_a_lie_msg(msg_size,d);
				return readBuffer(d);
			}
			//allocate a new buffer to fill the message
			byte[] msg_buffer = new byte[msg_size];
			int offset = 0;
			while (offset < msg_size)
			{
				int read = 0;
				try
				{
					read = d.plaintextStream.Read(msg_buffer,offset,msg_size - offset);
				}
				catch (System.IO.IOException ex)
				{
					if (ex.InnerException is System.Threading.ThreadAbortException) {} //swallow
					else throw ex;
				}
				
				if (read==0)
					d.raiseReadZeroBytes();
				offset += read;
			}
			//logArray(msg_buffer,"ciphertext");
			byte[] plaintext = new byte[0];
			try
			{
				plaintext = cryptlib.AES.decrypt(msg_buffer,d.aeskey);
			}
			catch (System.Security.Cryptography.CryptographicException e)
			{
				logger.logger.warn("Had a problem decrypting the data.  Acting like the connection was closed.");
				d.raiseReadZeroBytes();
			}
			
			return plaintext;
		}

		int readSize (pipette.deepID d)
		{
			byte[] intbuf = new byte[int_32_size];
			int offset = 0;
			while (offset < int_32_size) {
				int read = 0;
				try {
					read = d.plaintextStream.Read (intbuf, offset, int_32_size - offset);
				} catch (System.IO.IOException ex) {
					if (ex.InnerException is System.Threading.ThreadAbortException) {
					} else
						throw ex;
				}
				if (read == 0)
					d.raiseReadZeroBytes ();
				offset += read;
			}
		return System.BitConverter.ToInt32(intbuf,0);

		}
		public void sendCakeIsALieMsg(int msg,deepID d)
		{
			logger.logger.debug("Sending cakeisaliemsg: " + msg);
			byte[] length = System.BitConverter.GetBytes(msg);
			d.plaintextStream.Write(length,0,length.Length);
			d.plaintextStream.Flush();
		}
		public void spinForResumeMsg(deepID d)
		{
			int a = readSize(d);
			if (a != AuthenticationTypeAESLike.remote_end_resumed)
			{
				logger.logger.FAIL("I got some message, but it wasn't the resume I was expecting: " + a);
			}
		}
		public override void writeBuffer (byte[] b, int length,deepID d)
		{
			logger.logger.debug("Sending message of length " + length);
			if (b.Length > max_msg_size) throw new Exception("Exceeded max AES message size");
			byte[] ciphertext = cryptlib.AES.encrypt(b,length,d.aeskey);
			byte[] clength = System.BitConverter.GetBytes(ciphertext.Length);
			d.plaintextStream.Write(clength,0,clength.Length);
			d.plaintextStream.Write(ciphertext,0,ciphertext.Length);
			d.plaintextStream.Flush();
			logArray(ciphertext,"writingCyphertext");
		}


	}
}
