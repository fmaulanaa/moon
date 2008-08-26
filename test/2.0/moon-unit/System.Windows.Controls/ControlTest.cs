using System;
using System.Net;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Documents;
using System.Windows.Ink;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Animation;
using System.Windows.Shapes;
using System.Collections.Generic;
using Mono.Moonlight.UnitTesting;


namespace MoonTest.System.Windows.Controls
{
	[TestClass]
	public class ControlTest
	{
		class ControlPoker : UserControl {
			public void SetContent (UIElement ui)
			{
				Content = ui;
			}
		}

		class DefaultStyleKey_TypeClass : UserControl {
			public DefaultStyleKey_TypeClass ()
			{
				DefaultStyleKey = typeof (DefaultStyleKey_TypeClass);
			}
		}

		class DefaultStyleKey_TypeClass2 : UserControl {
			public DefaultStyleKey_TypeClass2 ()
			{
				DefaultStyleKey = typeof (UserControl);
			}
		}

		class DefaultStyleKey_NullClass : UserControl {
			public DefaultStyleKey_NullClass ()
			{
				DefaultStyleKey = null;
			}
		}

		class DefaultStyleKey_DifferentTypeClass : UserControl {
			public DefaultStyleKey_DifferentTypeClass ()
			{
				DefaultStyleKey = typeof (Button);
			}
		}

		class DefaultStyleKey_NonDOTypeClass : UserControl {
			public DefaultStyleKey_NonDOTypeClass ()
			{
				DefaultStyleKey = typeof (string);
			}
		}

		class DefaultStyleKey_NonTypeClass : UserControl {
			public DefaultStyleKey_NonTypeClass ()
			{
				DefaultStyleKey = "hi";
			}
		}

		class DefaultStyleKey_GetterClass : UserControl {
			public DefaultStyleKey_GetterClass ()
			{
			}

			public Type GetDefaultStyleKey()
			{
				return (Type)DefaultStyleKey;
			}
		}

		[TestMethod]
		public void DefaultStyleKeyTest ()
		{
			DefaultStyleKey_GetterClass gc = new DefaultStyleKey_GetterClass ();
			Assert.AreEqual (null, gc.GetDefaultStyleKey());

			Assert.Throws (delegate { DefaultStyleKey_TypeClass tc = new DefaultStyleKey_TypeClass (); },
				       typeof (ArgumentException), "1");
			Assert.Throws (delegate { DefaultStyleKey_TypeClass2 tc = new DefaultStyleKey_TypeClass2 (); },
				       typeof (ArgumentException), "2");
			Assert.Throws (delegate { DefaultStyleKey_NullClass nc = new DefaultStyleKey_NullClass (); },
				       typeof (ArgumentException), "3");
			Assert.Throws (delegate { DefaultStyleKey_DifferentTypeClass dtc = new DefaultStyleKey_DifferentTypeClass (); },
				       typeof (InvalidOperationException), "4");
			Assert.Throws (delegate { DefaultStyleKey_NonDOTypeClass ndotc = new DefaultStyleKey_NonDOTypeClass (); },
				       typeof (ArgumentException), "5");
			Assert.Throws (delegate { DefaultStyleKey_NonTypeClass ntc = new DefaultStyleKey_NonTypeClass (); },
				       typeof (ArgumentException), "6");
		}

		[TestMethod]
		public void DefaultDesiredSizeTest ()
		{
 			ControlPoker p = new ControlPoker ();
 			Assert.AreEqual (new Size (0,0), p.DesiredSize);
		}

		[TestMethod]
		public void ChildlessMeasureTest ()
		{
 			ControlPoker p = new ControlPoker ();

			Size s = new Size (10,10);

			p.Measure (s);

			Assert.AreEqual (new Size (0,0), p.DesiredSize, "DesiredSize");
		}

		[TestMethod]
		public void ChildlessMarginMeasureTest ()
		{
 			ControlPoker p = new ControlPoker ();

			p.Margin = new Thickness (10);

			p.Measure (new Size (10, 10));

			Assert.AreEqual (new Size (10,10), p.DesiredSize, "DesiredSize");
		}
		
		[TestMethod]
		public void ChildlessMinWidthMeasureTest1 ()
		{
 			ControlPoker p = new ControlPoker ();

			p.MinWidth = 50;

			Size s = new Size (10,10);

			p.Measure (s);

			Assert.AreEqual (10, p.DesiredSize.Width);
		}

		[TestMethod]
		public void ChildlessMinWidthMeasureTest2 ()
		{
 			ControlPoker p = new ControlPoker ();

			p.MinWidth = 5;

			Size s = new Size (10,10);

			p.Measure (s);

			Assert.AreEqual (5, p.DesiredSize.Width);
		}

		[TestMethod]
		public void ChildlessMinHeightMeasureTest1 ()
		{
 			ControlPoker p = new ControlPoker ();

			p.MinHeight = 50;

			Size s = new Size (10,10);

			p.Measure (s);

			Assert.AreEqual (10, p.DesiredSize.Height);
		}

		[TestMethod]
		public void ChildlessMinHeightMeasureTest2 ()
		{
 			ControlPoker p = new ControlPoker ();

			p.MinHeight = 5;

			Size s = new Size (10,10);

			p.Measure (s);

			Assert.AreEqual (5, p.DesiredSize.Height);
		}

		[TestMethod]
		public void ChildMeasureTest1 ()
		{
			ControlPoker p = new ControlPoker ();
			Rectangle r = new Rectangle();

			p.SetContent (r);

			r.Width = 50;
			r.Height = 50;

			p.Measure (new Size (10, 10));

			Assert.AreEqual (new Size (10,10), p.DesiredSize);
		}

		[TestMethod]
		public void ChildMeasureTest2 ()
		{
			ControlPoker p = new ControlPoker ();
			Rectangle r = new Rectangle();

			p.SetContent (r);

			r.Width = 50;
			r.Height = 50;

			p.Measure (new Size (100, 100));

			Assert.AreEqual (new Size (50,50), p.DesiredSize);
		}

		[TestMethod]
		public void ChildThicknessMeasureTest1 ()
		{
			ControlPoker p = new ControlPoker ();
			Rectangle r = new Rectangle();

			p.Margin = new Thickness (5);
			p.SetContent (r);

			r.Width = 50;
			r.Height = 50;

			p.Measure (new Size (10, 10));

			Assert.AreEqual (new Size (10,10), p.DesiredSize);
		}

		[TestMethod]
		public void ChildThicknessMeasureTest2 ()
		{
			ControlPoker p = new ControlPoker ();
			Rectangle r = new Rectangle();

			p.Margin = new Thickness (5);
			p.SetContent (r);

			r.Width = 50;
			r.Height = 50;

			p.Measure (new Size (100, 100));

			Assert.AreEqual (new Size (60,60), p.DesiredSize);
		}

		[TestMethod]
		public void GetTemplateChildTest ()
		{
		}
	}
}
