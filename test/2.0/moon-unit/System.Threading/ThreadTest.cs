//Permission is hereby granted, free of charge, to any person obtaining
//a copy of this software and associated documentation files (the
//"Software"), to deal in the Software without restriction, including
//without limitation the rights to use, copy, modify, merge, publish,
//distribute, sublicense, and/or sell copies of the Software, and to
//permit persons to whom the Software is furnished to do so, subject to
//the following conditions:
//
//The above copyright notice and this permission notice shall be
//included in all copies or substantial portions of the Software.
//
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
//EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
//MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
//NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
//LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
//OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
//WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//Copyright (c) 2008 Novell, Inc.
//
//Authors:
//	Rolf Bjarne Kvinge  (RKvinge@novell.com)
//

using System;
using System.Windows;
using System.Windows.Controls;
using System.Threading;
using System.Diagnostics;
using System.Windows.Threading;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Mono.Moonlight.UnitTesting;
using Microsoft.Silverlight.Testing;
using System.Windows.Shapes;
using System.Windows.Media.Animation;

namespace MoonTest.System.Threading
{
	[TestClass]
	public class ThreadTest : SilverlightTest
	{
		static EventHandler<ApplicationUnhandledExceptionEventArgs> ex_handler;

		[TestMethod]
		[MoonlightBug]
		public void BeginInvokeTest ()
		{
			AsyncCallback c = delegate { };
			Assert.Throws<NotSupportedException> (() => c.BeginInvoke (null, null, null), "#1");
			Assert.Throws<NotSupportedException> (() => c.EndInvoke (null), "#2");
		}

		[TestMethod]
		[Asynchronous]
		public void ThreadExceptionTest ()
		{
			bool unhandled_exception_raised = false;
			Dispatcher dispatcher;

			dispatcher = Mono.Moonlight.UnitTesting.App.Instance.RootVisual.Dispatcher;

			Thread t = new Thread (delegate () { throw new Exception ("This shouldn't crash the process"); });

			ex_handler = delegate (object sender, ApplicationUnhandledExceptionEventArgs e) {
				unhandled_exception_raised = true;
				e.Handled = true;
				dispatcher.BeginInvoke (delegate () 
				{
					Mono.Moonlight.UnitTesting.Testing.CustomUnhandledExceptionHandler -= ex_handler;
					ex_handler = null;
				});
			};
			Mono.Moonlight.UnitTesting.Testing.CustomUnhandledExceptionHandler += ex_handler;
			
			EnqueueConditional (() => unhandled_exception_raised);
			EnqueueConditional (() => ex_handler == null); // wait until the cleanup has happened too
			EnqueueTestComplete ();

			t.Start ();
		}

		[TestMethod]
		[Asynchronous]
		[ExpectedException (typeof (InvalidOperationException))]
		[MoonlightBug]
		public void ThreadExceptionTest2 ()
		{
			bool unhandled_exception_raised = false;
			Thread t = new Thread (delegate () { throw new InvalidOperationException ("This shouldn't be caught"); });

			App app = Mono.Moonlight.UnitTesting.App.Instance;
			app.CustomUnhandledExceptionHandler += delegate (object sender, ApplicationUnhandledExceptionEventArgs e) {
				unhandled_exception_raised = true;
			};

			EnqueueSleep (500);
			Enqueue (() => Assert.IsFalse (unhandled_exception_raised, "#1"));
			EnqueueTestComplete ();

			t.Start ();
		}
	}
}
